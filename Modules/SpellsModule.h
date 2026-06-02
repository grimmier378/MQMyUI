#pragma once

#include "../Core/ModuleBase.h"
#include "SpellPicker.h"

#include <mq/Plugin.h>

#include <string>

class SpellsModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Spells"; }

	void OnInit() override;
	void OnPulse() override;
	void OnRenderGUI() override;

private:
	int GemCount() const;
	void DrawOptionsButton();
	void OpenPickerForGem(int gemIndex);

	SpellPicker m_picker;
	int   m_memGemIndex = 0;
};
