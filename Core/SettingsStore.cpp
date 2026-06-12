#include "SettingsStore.h"
#include "ChatBridge.h"

#include <mq/Plugin.h>

#include <charconv>

namespace
{
constexpr char kInsertSettingSql[] =
	"INSERT OR REPLACE INTO settings (server, character, module, name, type, value) "
	"VALUES (?, ?, ?, ?, ?, ?)";

// Returns the column as a std::string, mapping a SQL NULL to "".
std::string ColText(sqlite3_stmt* stmt, int col)
{
	const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
	return text ? text : "";
}

// Binds the (server, character, module, name) key columns starting at parameter idx.
void BindContext(sqlite3_stmt* stmt, int idx, const std::string& server, const std::string& character,
	const std::string& module, const std::string& name)
{
	sqlite3_bind_text(stmt, idx + 0, server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, idx + 1, character.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, idx + 2, module.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, idx + 3, name.c_str(), -1, SQLITE_TRANSIENT);
}
} // namespace

SettingsStore::~SettingsStore()
{
	Close();
}

bool SettingsStore::Open(const std::string& dbPath)
{
	if (m_db)
	{
		Close();
	}

	int rc = sqlite3_open_v2(dbPath.c_str(), &m_db,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != SQLITE_OK)
	{
		myui::ChatOutf("\ar[MyUI] Failed to open settings database: %s", sqlite3_errmsg(m_db));
		sqlite3_close(m_db);
		m_db = nullptr;
		return false;
	}

	ExecSQL("PRAGMA journal_mode=WAL");
	ExecSQL("PRAGMA busy_timeout=3000");

	InitSchema();
	return true;
}

void SettingsStore::Close()
{
	if (m_db)
	{
		Flush();
		sqlite3_close(m_db);
		m_db = nullptr;
	}
	m_cache.clear();
}

void SettingsStore::ExecSQL(const char* sql)
{
	if (!m_db)
	{
		return;
	}

	char* errMsg = nullptr;
	int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		myui::ChatOutf("\ar[MyUI] SQL error: %s", errMsg ? errMsg : "unknown");
		if (errMsg)
		{
			sqlite3_free(errMsg);
		}
	}
}

bool SettingsStore::PrepareAndStep(const char* sql, sqlite3_stmt*& stmt)
{
	if (!m_db)
	{
		return false;
	}

	int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK)
	{
		myui::ChatOutf("\ar[MyUI] Prepare failed: %s", sqlite3_errmsg(m_db));
		stmt = nullptr;
		return false;
	}
	return true;
}

int SettingsStore::GetSchemaVersion()
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep("SELECT version FROM schema_version WHERE id=1", stmt))
	{
		return 0;
	}

	int version = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		version = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	return version;
}

void SettingsStore::SetSchemaVersion(int version)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep("UPDATE schema_version SET version=? WHERE id=1", stmt))
	{
		return;
	}

	sqlite3_bind_int(stmt, 1, version);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

void SettingsStore::RunMigrations()
{
	int currentVersion = GetSchemaVersion();
	if (currentVersion < 1)
	{
		SetSchemaVersion(1);
	}
	if (currentVersion < 2)
	{
		ExecSQL("DROP TABLE IF EXISTS themes");
		SetSchemaVersion(2);
	}
	if (currentVersion < 3)
	{
		ExecSQL("UPDATE settings SET module='XPBars' WHERE module='AAParty'");
		SetSchemaVersion(3);
	}
	if (currentVersion < 4)
	{
		ExecSQL(
			"CREATE TABLE IF NOT EXISTS spell_sets ("
			"server TEXT NOT NULL, character TEXT NOT NULL, set_name TEXT NOT NULL, "
			"gem_slot INTEGER NOT NULL, spell_id INTEGER NOT NULL, "
			"PRIMARY KEY (server, character, set_name, gem_slot))");
		SetSchemaVersion(4);
	}
	if (currentVersion < 5)
	{
		// Modules were renamed; relabel their saved rows (all characters at once) so
		// settings are preserved, mirroring the AAParty->XPBars migration above. OR IGNORE
		// keeps an existing new-name row on conflict, then the stale old-name row is dropped.
		const char* renames[][2] = {
			{ "MyAA", "AA" }, { "MyInventory", "Inventory" },
			{ "ThemeZ", "ThemeEditor" }, { "Themes", "ThemeEditor" },
		};
		for (const auto& r : renames)
		{
			ExecSQL(fmt::format("UPDATE OR IGNORE settings SET module='{}' WHERE module='{}'", r[1], r[0]).c_str());
			ExecSQL(fmt::format("DELETE FROM settings WHERE module='{}'", r[0]).c_str());
		}
		SetSchemaVersion(5);
	}
	if (currentVersion < 6)
	{
		ExecSQL(
			"CREATE TABLE IF NOT EXISTS itrack_items ("
			"ordinal INTEGER NOT NULL, item_name TEXT NOT NULL, PRIMARY KEY (item_name))");
		// Migrate the old pipe-joined global iTrack list into rows, then drop the old setting.
		std::string raw = GetGlobal("iTrack", "TrackedList", "");
		if (!raw.empty())
		{
			ExecSQL("BEGIN");
			int ord = 0;
			for (const std::string& token : mq::split(raw, '|'))
			{
				if (token.empty())
				{
					continue;
				}
				sqlite3_stmt* ins = nullptr;
				if (PrepareAndStep("INSERT OR IGNORE INTO itrack_items (ordinal, item_name) VALUES (?, ?)", ins))
				{
					sqlite3_bind_int(ins, 1, ord++);
					sqlite3_bind_text(ins, 2, token.c_str(), -1, SQLITE_TRANSIENT);
					sqlite3_step(ins);
					sqlite3_finalize(ins);
				}
			}
			ExecSQL("COMMIT");
			ExecSQL("DELETE FROM settings WHERE module='iTrack' AND name='TrackedList'");
		}
		SetSchemaVersion(6);
	}
}

void SettingsStore::InitSchema()
{
	ExecSQL(
		"CREATE TABLE IF NOT EXISTS schema_version ("
		"id INTEGER PRIMARY KEY CHECK (id = 1), "
		"version INTEGER NOT NULL DEFAULT 0)");
	ExecSQL("INSERT OR IGNORE INTO schema_version (id, version) VALUES (1, 0)");

	ExecSQL(
		"CREATE TABLE IF NOT EXISTS settings ("
		"server TEXT NOT NULL, character TEXT NOT NULL, module TEXT NOT NULL, name TEXT NOT NULL, "
		"type TEXT NOT NULL CHECK(type IN ('string','number','boolean')), value TEXT NOT NULL, "
		"PRIMARY KEY (server, character, module, name))");

	RunMigrations();

	ExecSQL(
		"CREATE TABLE IF NOT EXISTS themes ("
		"theme TEXT NOT NULL, name TEXT NOT NULL, "
		"type TEXT NOT NULL CHECK(type IN ('imvec4','imvec2','float','int','bool')), value TEXT NOT NULL, "
		"PRIMARY KEY (theme, name))");

	ExecSQL(
		"CREATE TABLE IF NOT EXISTS spell_sets ("
		"server TEXT NOT NULL, character TEXT NOT NULL, set_name TEXT NOT NULL, "
		"gem_slot INTEGER NOT NULL, spell_id INTEGER NOT NULL, "
		"PRIMARY KEY (server, character, set_name, gem_slot))");

	ExecSQL(
		"CREATE TABLE IF NOT EXISTS itrack_items ("
		"ordinal INTEGER NOT NULL, item_name TEXT NOT NULL, PRIMARY KEY (item_name))");
}

void SettingsStore::SetContext(const std::string& server, const std::string& character)
{
	if (server == m_server && character == m_character)
	{
		return;
	}

	Flush();
	m_cache.clear();
	m_server = server;
	m_character = character;
}

std::string SettingsStore::MakeKey(const std::string& module, const std::string& name) const
{
	std::string key;
	key.reserve(module.size() + name.size() + 1);
	key.append(module);
	key.push_back('\x1f');
	key.append(name);
	return key;
}

std::string SettingsStore::Fetch(const std::string& module, const std::string& name, bool& found)
{
	found = false;
	std::string key = MakeKey(module, name);

	auto it = m_cache.find(key);
	if (it != m_cache.end() && it->second.loaded)
	{
		found = true;
		return it->second.value;
	}

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"SELECT value FROM settings WHERE server=? AND character=? AND module=? AND name=?", stmt))
	{
		return std::string();
	}

	BindContext(stmt, 1, m_server, m_character, module, name);

	std::string value;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		value = ColText(stmt, 0);
		found = true;
		m_cache[key] = CacheEntry{ value, true };
	}

	sqlite3_finalize(stmt);
	return value;
}

void SettingsStore::WriteRow(const std::string& module, const std::string& name, const char* type, const std::string& value)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(kInsertSettingSql, stmt))
	{
		return;
	}

	BindContext(stmt, 1, m_server, m_character, module, name);
	sqlite3_bind_text(stmt, 5, type, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, value.c_str(), -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) == SQLITE_DONE)
	{
		m_cache[MakeKey(module, name)] = CacheEntry{ value, true };
	}
	sqlite3_finalize(stmt);
}

std::string SettingsStore::GetString(const std::string& module, const std::string& name, const std::string& defaultVal)
{
	bool found = false;
	std::string value = Fetch(module, name, found);
	return found ? value : defaultVal;
}

void SettingsStore::SetString(const std::string& module, const std::string& name, const std::string& value)
{
	WriteRow(module, name, "string", value);
}

float SettingsStore::GetNumber(const std::string& module, const std::string& name, float defaultVal)
{
	bool found = false;
	std::string value = Fetch(module, name, found);
	if (!found)
	{
		return defaultVal;
	}

	return GetFloatFromString(value, defaultVal);
}

void SettingsStore::SetNumber(const std::string& module, const std::string& name, float value)
{
	char buf[32];
	auto res = std::to_chars(buf, buf + sizeof(buf), value);
	WriteRow(module, name, "number", std::string(buf, res.ptr));
}

bool SettingsStore::GetBool(const std::string& module, const std::string& name, bool defaultVal)
{
	bool found = false;
	std::string value = Fetch(module, name, found);
	if (!found)
	{
		return defaultVal;
	}
	return value == "1" || value == "true";
}

void SettingsStore::SetBool(const std::string& module, const std::string& name, bool value)
{
	WriteRow(module, name, "boolean", value ? "1" : "0");
}

void SettingsStore::Flush()
{
	if (m_db)
	{
		ExecSQL("PRAGMA wal_checkpoint(PASSIVE)");
	}
}

void SettingsStore::InvalidateCache()
{
	m_cache.clear();
}

void SettingsStore::BeginTransaction()
{
	ExecSQL("BEGIN TRANSACTION");
}

void SettingsStore::CommitTransaction()
{
	ExecSQL("COMMIT");
}

void SettingsStore::RollbackTransaction()
{
	ExecSQL("ROLLBACK");
}

void SettingsStore::CopyCharacterSettings(const std::string& fromServer, const std::string& fromChar,
	const std::string& toServer, const std::string& toChar)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"INSERT OR REPLACE INTO settings (server, character, module, name, type, value) "
		"SELECT ?, ?, module, name, type, value FROM settings WHERE server=? AND character=?", stmt))
	{
		return;
	}

	sqlite3_bind_text(stmt, 1, toServer.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, toChar.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, fromServer.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, fromChar.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (toServer == m_server && toChar == m_character)
	{
		m_cache.clear();
	}
}

std::vector<CharIdent> SettingsStore::GetCharacterList()
{
	std::vector<CharIdent> chars;

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"SELECT DISTINCT server, character FROM settings "
		"WHERE NOT (server='' AND character='') ORDER BY server, character", stmt))
	{
		return chars;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		chars.push_back(CharIdent{ ColText(stmt, 0), ColText(stmt, 1) });
	}

	sqlite3_finalize(stmt);
	return chars;
}

std::vector<SettingRow> SettingsStore::GetAllSettings(const std::string& server, const std::string& character)
{
	std::vector<SettingRow> rows;

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"SELECT module, name, type, value FROM settings WHERE server=? AND character=? "
		"ORDER BY module, name", stmt))
	{
		return rows;
	}

	sqlite3_bind_text(stmt, 1, server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, character.c_str(), -1, SQLITE_TRANSIENT);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		rows.push_back(SettingRow{ ColText(stmt, 0), ColText(stmt, 1), ColText(stmt, 2), ColText(stmt, 3) });
	}

	sqlite3_finalize(stmt);
	return rows;
}

void SettingsStore::SetRaw(const std::string& server, const std::string& character,
	const std::string& module, const std::string& name, const std::string& type, const std::string& value)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(kInsertSettingSql, stmt))
	{
		return;
	}

	BindContext(stmt, 1, server, character, module, name);
	sqlite3_bind_text(stmt, 5, type.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, value.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (server == m_server && character == m_character)
	{
		m_cache.erase(MakeKey(module, name));
	}
}

std::vector<SettingRow> SettingsStore::ApplySettingRows(const std::string& server, const std::string& character,
	const std::vector<SettingRow>& rows)
{
	if (rows.empty())
	{
		return {};
	}

	BeginTransaction();
	for (const SettingRow& row : rows)
	{
		if (row.module.empty() || row.name.empty())
		{
			continue;
		}
		SetRaw(server, character, row.module, row.name, row.type, row.value);
	}
	CommitTransaction();

	std::vector<SettingRow> current = GetAllSettings(server, character);

	std::vector<SettingRow> result;
	result.reserve(rows.size());
	for (const SettingRow& row : rows)
	{
		if (row.module.empty() || row.name.empty())
		{
			continue;
		}
		for (const SettingRow& cur : current)
		{
			if (cur.module == row.module && cur.name == row.name)
			{
				result.push_back(cur);
				break;
			}
		}
	}
	return result;
}

void SettingsStore::DeleteSetting(const std::string& module, const std::string& name)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"DELETE FROM settings WHERE server=? AND character=? AND module=? AND name=?", stmt))
	{
		return;
	}

	BindContext(stmt, 1, m_server, m_character, module, name);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	m_cache.erase(MakeKey(module, name));
}

std::string SettingsStore::GetGlobal(const std::string& module, const std::string& name, const std::string& defaultVal)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"SELECT value FROM settings WHERE server='' AND character='' AND module=? AND name=?", stmt))
	{
		return defaultVal;
	}

	sqlite3_bind_text(stmt, 1, module.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);

	std::string value = defaultVal;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		value = ColText(stmt, 0);
	}

	sqlite3_finalize(stmt);
	return value;
}

void SettingsStore::SetGlobal(const std::string& module, const std::string& name, const std::string& value)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"INSERT OR REPLACE INTO settings (server, character, module, name, type, value) "
		"VALUES ('', '', ?, ?, 'string', ?)", stmt))
	{
		return;
	}

	sqlite3_bind_text(stmt, 1, module.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, value.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

void SettingsStore::SaveTheme(const std::string& themeName, const std::vector<ThemeRow>& rows)
{
	if (!m_db)
	{
		return;
	}

	BeginTransaction();

	sqlite3_stmt* del = nullptr;
	if (PrepareAndStep("DELETE FROM themes WHERE theme=?", del))
	{
		sqlite3_bind_text(del, 1, themeName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(del);
		sqlite3_finalize(del);
	}

	bool ok = true;
	for (const ThemeRow& row : rows)
	{
		sqlite3_stmt* stmt = nullptr;
		if (!PrepareAndStep(
			"INSERT INTO themes (theme, name, type, value) VALUES (?, ?, ?, ?)", stmt))
		{
			ok = false;
			break;
		}

		sqlite3_bind_text(stmt, 1, themeName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, row.name.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, row.type.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, row.value.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (ok)
	{
		CommitTransaction();
	}
	else
	{
		RollbackTransaction();
	}
}

std::vector<ThemeRow> SettingsStore::LoadTheme(const std::string& themeName)
{
	std::vector<ThemeRow> rows;

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep("SELECT name, type, value FROM themes WHERE theme=?", stmt))
	{
		return rows;
	}

	sqlite3_bind_text(stmt, 1, themeName.c_str(), -1, SQLITE_TRANSIENT);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		rows.push_back(ThemeRow{ ColText(stmt, 0), ColText(stmt, 1), ColText(stmt, 2) });
	}

	sqlite3_finalize(stmt);
	return rows;
}

std::vector<std::string> SettingsStore::GetThemeNames()
{
	std::vector<std::string> names;

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep("SELECT DISTINCT theme FROM themes ORDER BY theme", stmt))
	{
		return names;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		names.push_back(ColText(stmt, 0));
	}

	sqlite3_finalize(stmt);
	return names;
}

void SettingsStore::DeleteTheme(const std::string& themeName)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep("DELETE FROM themes WHERE theme=?", stmt))
	{
		return;
	}

	sqlite3_bind_text(stmt, 1, themeName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

void SettingsStore::SaveSpellSet(const std::string& setName, const std::vector<SpellSetRow>& rows)
{
	if (!m_db)
	{
		return;
	}

	BeginTransaction();

	sqlite3_stmt* del = nullptr;
	if (PrepareAndStep("DELETE FROM spell_sets WHERE server=? AND character=? AND set_name=?", del))
	{
		sqlite3_bind_text(del, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(del, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(del, 3, setName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(del);
		sqlite3_finalize(del);
	}

	bool ok = true;
	for (const SpellSetRow& row : rows)
	{
		sqlite3_stmt* stmt = nullptr;
		if (!PrepareAndStep(
			"INSERT INTO spell_sets (server, character, set_name, gem_slot, spell_id) VALUES (?, ?, ?, ?, ?)", stmt))
		{
			ok = false;
			break;
		}

		sqlite3_bind_text(stmt, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, setName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, row.gemSlot);
		sqlite3_bind_int(stmt, 5, row.spellId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (ok)
	{
		CommitTransaction();
	}
	else
	{
		RollbackTransaction();
	}
}

std::vector<SpellSetRow> SettingsStore::LoadSpellSet(const std::string& setName)
{
	std::vector<SpellSetRow> rows;

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"SELECT gem_slot, spell_id FROM spell_sets WHERE server=? AND character=? AND set_name=? ORDER BY gem_slot", stmt))
	{
		return rows;
	}

	sqlite3_bind_text(stmt, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, setName.c_str(), -1, SQLITE_TRANSIENT);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		SpellSetRow row;
		row.gemSlot = sqlite3_column_int(stmt, 0);
		row.spellId = sqlite3_column_int(stmt, 1);
		rows.push_back(row);
	}

	sqlite3_finalize(stmt);
	return rows;
}

std::vector<std::string> SettingsStore::GetSpellSetNames()
{
	std::vector<std::string> names;

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"SELECT DISTINCT set_name FROM spell_sets WHERE server=? AND character=? ORDER BY set_name", stmt))
	{
		return names;
	}

	sqlite3_bind_text(stmt, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		names.push_back(ColText(stmt, 0));
	}

	sqlite3_finalize(stmt);
	return names;
}

void SettingsStore::DeleteSpellSet(const std::string& setName)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep("DELETE FROM spell_sets WHERE server=? AND character=? AND set_name=?", stmt))
	{
		return;
	}

	sqlite3_bind_text(stmt, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, setName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

std::vector<std::string> SettingsStore::LoadTrackedItems()
{
	std::vector<std::string> items;

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep("SELECT item_name FROM itrack_items ORDER BY ordinal", stmt))
	{
		return items;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		items.push_back(ColText(stmt, 0));
	}

	sqlite3_finalize(stmt);
	return items;
}

void SettingsStore::SaveTrackedItems(const std::vector<std::string>& items)
{
	if (!m_db)
	{
		return;
	}

	BeginTransaction();
	ExecSQL("DELETE FROM itrack_items");

	int ord = 0;
	bool ok = true;
	for (const std::string& name : items)
	{
		if (name.empty())
		{
			continue;
		}

		sqlite3_stmt* stmt = nullptr;
		if (!PrepareAndStep("INSERT OR IGNORE INTO itrack_items (ordinal, item_name) VALUES (?, ?)", stmt))
		{
			ok = false;
			break;
		}

		sqlite3_bind_int(stmt, 1, ord++);
		sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (ok)
	{
		CommitTransaction();
	}
	else
	{
		RollbackTransaction();
	}
}
