#pragma once

#include <mq/Plugin.h>

#include <map>
#include <string>
#include <vector>

class SettingsStore;

struct Theme
{
	std::string name;
	ImVec4 colors[ImGuiCol_COUNT];

	float windowRounding    = 0.0f;
	float frameRounding     = 0.0f;
	float childRounding     = 0.0f;
	float popupRounding     = 0.0f;
	float scrollbarRounding = 0.0f;
	float grabRounding      = 0.0f;
	float tabRounding       = 4.0f;

	float windowBorderSize  = 1.0f;
	float frameBorderSize   = 0.0f;
	float childBorderSize   = 1.0f;
	float popupBorderSize   = 1.0f;
	float tabBarBorderSize  = 1.0f;

	float indentSpacing     = 21.0f;
	float scrollbarSize     = 14.0f;
	float grabMinSize       = 12.0f;

	ImVec2 windowPadding    = ImVec2(8.0f, 8.0f);
	ImVec2 framePadding     = ImVec2(4.0f, 3.0f);
	ImVec2 cellPadding      = ImVec2(4.0f, 2.0f);
	ImVec2 itemSpacing      = ImVec2(8.0f, 4.0f);
	ImVec2 itemInnerSpacing = ImVec2(4.0f, 4.0f);

	bool isEmpty     = false;
	bool initialized = false;
};

class ThemeManager
{
public:
	void Init(SettingsStore* store);

	void LoadAll();
	const Theme* GetTheme(const std::string& name) const;
	std::vector<std::string> GetThemeNames() const;
	const Theme& DefaultStyle() const { return m_defaultTheme; }

	ImGuiStyle ApplyTheme(const Theme& theme) const;
	static void ResetTheme(const ImGuiStyle& old);

	void SaveTheme(const Theme& theme);
	void DeleteTheme(const std::string& name);

	void SetActiveTheme(const std::string& name);
	void LoadActiveTheme();
	const std::string& ActiveName() const { return m_activeName; }

	void SetPreview(const Theme* theme) { m_preview = theme; }
	void ClearPreview() { m_preview = nullptr; }
	const Theme& Resolved() const;

private:
	void CaptureImGuiDefault();
	void EnsureDefaultSentinel();
	void SeedBuiltins();
	Theme MakeGrape() const;
	Theme MakeBurnt() const;

	SettingsStore* m_store = nullptr;
	std::map<std::string, Theme> m_themes;
	Theme m_defaultTheme;
	std::string m_activeName = "Default";
	const Theme* m_preview = nullptr;
};
