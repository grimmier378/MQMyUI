#pragma once

#include "../Core/ModuleBase.h"

#include <mq/Plugin.h>

#include <string>

class PlayerModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Player"; }

	void OnRenderGUI() override;

private:
	void DrawPlayer();
	void DrawTarget();
	void DrawTargetBuffs();
};
