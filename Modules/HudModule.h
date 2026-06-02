#pragma once

#include "../Core/ModuleBase.h"

#include <mq/Plugin.h>

#include <string>

class HudModule : public ModuleBase
{
public:
	const char* GetName() const override { return "HUD"; }

	void OnRenderGUI() override;
	void OnShutdown() override;

private:
	void DrawStatusEffects();

	CTextureAnimation* m_statusIcon = nullptr;
};
