#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/ThemeManager.h"

#include <mq/Plugin.h>

#include <string>

// Reusable theme-editor sections shared by the standalone Themes window and the Settings window's
// General pane. They operate on caller-owned edit state so both call sites stay in sync.
namespace themeeditor
{
void LoadSelected(ThemeManager* themes, const std::string& name, Theme& edit, std::string& selected, char* nameBuf, size_t nameBufSize);
void DrawTopBar(ThemeManager* themes, Theme& edit, std::string& selected, char* nameBuf, size_t nameBufSize);
void DrawColorsSection(ThemeManager* themes, Theme& edit);
void DrawStylesSection(ThemeManager* themes, Theme& edit);
} // namespace themeeditor

class ThemeZModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Themes"; }

	void OnRenderGUI() override;
	void OnShutdown() override;

private:
	Theme m_edit;
	std::string m_selected = "Default";
	char m_nameBuf[64] = { 0 };
	bool m_wasOpen = false;
};
