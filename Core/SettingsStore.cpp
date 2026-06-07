#include "SettingsStore.h"
#include "ChatBridge.h"

#include <mq/Plugin.h>

#include <charconv>

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

	sqlite3_bind_text(stmt, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, module.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, name.c_str(), -1, SQLITE_TRANSIENT);

	std::string value;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		value = text ? text : "";
		found = true;
		m_cache[key] = CacheEntry{ value, true };
	}

	sqlite3_finalize(stmt);
	return value;
}

void SettingsStore::WriteRow(const std::string& module, const std::string& name, const char* type, const std::string& value)
{
	m_cache[MakeKey(module, name)] = CacheEntry{ value, true };

	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"INSERT OR REPLACE INTO settings (server, character, module, name, type, value) "
		"VALUES (?, ?, ?, ?, ?, ?)", stmt))
	{
		return;
	}

	sqlite3_bind_text(stmt, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, module.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, type, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, value.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
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
		const char* s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		const char* c = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		chars.push_back(CharIdent{ s ? s : "", c ? c : "" });
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
		const char* m = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		rows.push_back(SettingRow{ m ? m : "", n ? n : "", t ? t : "", v ? v : "" });
	}

	sqlite3_finalize(stmt);
	return rows;
}

void SettingsStore::SetRaw(const std::string& server, const std::string& character,
	const std::string& module, const std::string& name, const std::string& type, const std::string& value)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"INSERT OR REPLACE INTO settings (server, character, module, name, type, value) "
		"VALUES (?, ?, ?, ?, ?, ?)", stmt))
	{
		return;
	}

	sqlite3_bind_text(stmt, 1, server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, character.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, module.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, type.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, value.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (server == m_server && character == m_character)
	{
		m_cache.erase(MakeKey(module, name));
	}
}

void SettingsStore::DeleteSetting(const std::string& module, const std::string& name)
{
	sqlite3_stmt* stmt = nullptr;
	if (!PrepareAndStep(
		"DELETE FROM settings WHERE server=? AND character=? AND module=? AND name=?", stmt))
	{
		return;
	}

	sqlite3_bind_text(stmt, 1, m_server.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, m_character.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, module.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, name.c_str(), -1, SQLITE_TRANSIENT);
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
		const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		value = text ? text : "";
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

	ExecSQL("BEGIN");

	sqlite3_stmt* del = nullptr;
	if (PrepareAndStep("DELETE FROM themes WHERE theme=?", del))
	{
		sqlite3_bind_text(del, 1, themeName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(del);
		sqlite3_finalize(del);
	}

	for (const ThemeRow& row : rows)
	{
		sqlite3_stmt* stmt = nullptr;
		if (!PrepareAndStep(
			"INSERT INTO themes (theme, name, type, value) VALUES (?, ?, ?, ?)", stmt))
		{
			continue;
		}

		sqlite3_bind_text(stmt, 1, themeName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, row.name.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, row.type.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, row.value.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	ExecSQL("COMMIT");
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
		const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		rows.push_back(ThemeRow{ n ? n : "", t ? t : "", v ? v : "" });
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
		const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		if (text)
		{
			names.emplace_back(text);
		}
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
