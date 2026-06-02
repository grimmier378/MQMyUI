#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/UiConfig.h"

#include <mq/Plugin.h>

#include <string>

class SettingsModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Settings"; }

	void OnRenderGUI() override;

private:
	void DrawWindowsTab();
	void DrawBarsTab();
	void DrawScalingTab();
	void DrawDisplayTab();
	void DrawTogglesTab();
	void DrawThemeTab();
	void DrawTargetPicker();
	void DrawBarEditor(const std::string& window, const std::string& role);

	std::string m_barWindow;
	std::string m_barRole;

	UiConfig m_remote;
	UiConfig* m_active = nullptr;
	std::string m_targetServer;
	std::string m_targetChar;
	bool m_remoteActive = false;
};
