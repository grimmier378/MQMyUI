#pragma once

#include "../Core/ModuleBase.h"

#include <mq/Plugin.h>

#include <string>

class PetModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Pet"; }

	void OnRenderGUI() override;

private:
	void DrawPetButtons();
	void DrawPetBuffs();
};
