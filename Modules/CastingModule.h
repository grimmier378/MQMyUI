#pragma once

#include "../Core/ModuleBase.h"

#include <mq/Plugin.h>

#include <chrono>
#include <string>

class CastingModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Casting"; }

	void OnPulse() override;
	void OnRenderGUI() override;

private:
	bool  m_casting = false;
	bool  m_wasVisible = false;
	float m_castTotalMs = 0.0f;
	std::string m_spellName;
	std::chrono::steady_clock::time_point m_castStart;
};
