#pragma once

#include "../Core/ModuleBase.h"

#include <mq/Plugin.h>

#include <cstdint>
#include <string>

class CastingModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Casting"; }

	void OnPulse() override;
	void OnRenderGUI() override;

private:
	bool     m_casting = false;
	uint32_t m_castEta = 0;
	float    m_castTotalMs = 0.0f;
	std::string m_spellName;
};
