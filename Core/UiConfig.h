#pragma once

#include <mq/Plugin.h>

#include <map>
#include <string>
#include <vector>

class SettingsStore;

struct BarStyle
{
	bool  vertical = false;
	float height   = 18.0f;
	float width    = 0.0f;
	float padEnd   = 4.0f;
	float rounding = 0.0f;

	MQColor fillLow{ 190, 75, 75, 255 };
	MQColor fillHigh{ 190, 75, 75, 255 };
	MQColor bgColor{ 30, 32, 40, 255 };

	bool gradientOn   = false;
	int  gradientMode = 1;
	int  gradientDir  = 0;

	bool    border          = false;
	float   borderThickness = 1.0f;
	MQColor borderColor{ 200, 200, 200, 255 };

	int         textMode      = 0;
	std::string textFormat    = "%.0f%%";
	float       textScale     = 1.0f;
	bool        textDropShadow = true;

	bool  ticksOn       = false;
	float tickEvery     = 0.10f;
	float tickThickness = 1.0f;
	int   tickAlpha     = 80;

	bool  shimmerOn      = false;
	float shimmerSpeed   = 0.5f;
	float shimmerWidth   = 45.0f;
	bool  shimmerFollows = true;

	bool  glowOn        = false;
	float glowThickness = 0.0f;
	int   glowAlpha     = 90;

	float tweenSeconds = 0.0f;
};

struct WindowConfig
{
	bool  visible   = false;
	bool  locked    = false;
	float textScale = 1.0f;
	float iconScale = 1.0f;

	std::map<std::string, bool>        flags;
	std::map<std::string, float>       nums;
	std::map<std::string, std::string> strs;
	std::map<std::string, BarStyle>    bars;
};

class UiConfig
{
public:
	void Init(SettingsStore* store);
	void Load();
	void Save();
	void LoadForCharacter(SettingsStore* store, const std::string& server, const std::string& character);
	void SaveForCharacter(SettingsStore* store, const std::string& server, const std::string& character);
	void RequestSave();
	void RequestReload();
	void ProcessPending();
	void PersistVisibility(const std::string& window);
	void PersistField(const std::string& window, const std::string& name);

	WindowConfig& Window(const std::string& name);
	BarStyle&     Bar(const std::string& window, const std::string& role);

	bool  Flag(const std::string& window, const std::string& name, bool def);
	void  SetFlag(const std::string& window, const std::string& name, bool value);
	float Num(const std::string& window, const std::string& name, float def);
	void  SetNum(const std::string& window, const std::string& name, float value);
	std::string Str(const std::string& window, const std::string& name, const std::string& def);
	void        SetStr(const std::string& window, const std::string& name, const std::string& value);
	MQColor     Color(const std::string& window, const std::string& name, const MQColor& def);
	void        SetColor(const std::string& window, const std::string& name, const MQColor& value);

	bool ToggleVisible(const std::string& window);
	const std::vector<std::string>& WindowKeys() const { return m_order; }

	bool previewCasting = false;

private:
	void SeedDefaults();
	void EnsureWindow(const std::string& name);
	void PruneDefunct();

	SettingsStore* m_store = nullptr;
	std::map<std::string, WindowConfig> m_windows;
	std::vector<std::string> m_order;
	std::string m_loadedKey;
	bool m_savePending = false;
	bool m_reloadPending = false;
};
