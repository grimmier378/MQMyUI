#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/ThemeManager.h"

#include <mq/Plugin.h>

#include <string>

class ThemeZModule : public ModuleBase
{
public:
	const char* GetName() const override { return "ThemeZ"; }

	void OnRenderGUI() override;
	void OnShutdown() override;

private:
	void LoadSelected(const std::string& name);
	void DrawTopBar();
	void DrawColorsSection();
	void DrawStylesSection();

	Theme m_edit;
	std::string m_selected = "Default";
	char m_nameBuf[64] = { 0 };
	bool m_wasOpen = false;
};
