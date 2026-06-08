#pragma once

#include "sqlite3.h"

#include <string>
#include <unordered_map>
#include <vector>

struct ThemeRow
{
	std::string name;
	std::string type;
	std::string value;
};

struct CharIdent
{
	std::string server;
	std::string character;
};

struct SettingRow
{
	std::string module;
	std::string name;
	std::string type;
	std::string value;
};

struct SpellSetRow
{
	int gemSlot = 0;
	int spellId = 0;
};

class SettingsStore
{
public:
	SettingsStore() = default;
	~SettingsStore();

	bool Open(const std::string& dbPath);
	void Close();
	bool IsOpen() const { return m_db != nullptr; }

	void SetContext(const std::string& server, const std::string& character);
	const std::string& Server() const { return m_server; }
	const std::string& Character() const { return m_character; }

	std::string GetString(const std::string& module, const std::string& name, const std::string& defaultVal);
	void        SetString(const std::string& module, const std::string& name, const std::string& value);

	float GetNumber(const std::string& module, const std::string& name, float defaultVal);
	void  SetNumber(const std::string& module, const std::string& name, float value);

	bool GetBool(const std::string& module, const std::string& name, bool defaultVal);
	void SetBool(const std::string& module, const std::string& name, bool value);

	std::string GetGlobal(const std::string& module, const std::string& name, const std::string& defaultVal);
	void        SetGlobal(const std::string& module, const std::string& name, const std::string& value);

	void Flush();
	void InvalidateCache();

	void BeginTransaction();
	void CommitTransaction();

	void CopyCharacterSettings(const std::string& fromServer, const std::string& fromChar,
		const std::string& toServer, const std::string& toChar);

	std::vector<CharIdent>  GetCharacterList();
	std::vector<SettingRow> GetAllSettings(const std::string& server, const std::string& character);
	std::vector<SettingRow> ApplySettingRows(const std::string& server, const std::string& character,
		const std::vector<SettingRow>& rows);
	void SetRaw(const std::string& server, const std::string& character,
		const std::string& module, const std::string& name, const std::string& type, const std::string& value);
	void DeleteSetting(const std::string& module, const std::string& name);

	void SaveTheme(const std::string& themeName, const std::vector<ThemeRow>& rows);
	std::vector<ThemeRow> LoadTheme(const std::string& themeName);
	std::vector<std::string> GetThemeNames();
	void DeleteTheme(const std::string& themeName);

	void SaveSpellSet(const std::string& setName, const std::vector<SpellSetRow>& rows);
	std::vector<SpellSetRow> LoadSpellSet(const std::string& setName);
	std::vector<std::string> GetSpellSetNames();
	void DeleteSpellSet(const std::string& setName);

private:
	void ExecSQL(const char* sql);
	bool PrepareAndStep(const char* sql, sqlite3_stmt*& stmt);
	void InitSchema();
	int  GetSchemaVersion();
	void SetSchemaVersion(int version);
	void RunMigrations();

	std::string MakeKey(const std::string& module, const std::string& name) const;
	void WriteRow(const std::string& module, const std::string& name, const char* type, const std::string& value);

	struct CacheEntry
	{
		std::string value;
		bool        loaded = false;
	};

	std::string Fetch(const std::string& module, const std::string& name, bool& found);

	sqlite3* m_db = nullptr;
	std::string m_server;
	std::string m_character;
	std::unordered_map<std::string, CacheEntry> m_cache;
};
