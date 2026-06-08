#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/UiConfig.h"
#include "../Core/SettingsStore.h"
#include "../Core/ThemeManager.h"

#include <mq/Plugin.h>

#include <string>
#include <vector>

class SettingsModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Settings"; }

	void OnRenderGUI() override;

private:
	void DrawSettingsTab();
	void DrawGeneralDetail();
	void DrawWindowDetail(const std::string& name);
	void DrawBarsTab();
	void DrawBarEditor(const std::string& window, const std::string& role);
	void DrawTargetPicker();

	std::string m_selWindow; // "" = General entry

	std::string m_barWindow;
	std::string m_barRole;

	int m_settingsTab = 0; // 0 = Settings, 1 = Bars (pill tab bar)
	std::string m_openBarTree; // window whose bar-tree branch is expanded (exclusive accordion)

	// Folded ThemeZ editor state (shared draw helpers operate on this).
	Theme m_themeEdit;
	std::string m_themeSel = "Default";
	char m_themeNameBuf[64] = { 0 };
	bool m_themeLoaded = false;
	bool m_themePreviewActive = false;
	bool m_drewGeneralThisFrame = false;

	UiConfig m_remote;
	UiConfig* m_active = nullptr;
	std::string m_targetServer;
	std::string m_targetChar;
	bool m_remoteActive = false;

	// Baselines for computing changed rows to broadcast/push on save.
	std::vector<SettingRow> m_localBaseline;
	std::vector<SettingRow> m_remoteBaseline;
	bool m_wasVisible = false;
};
