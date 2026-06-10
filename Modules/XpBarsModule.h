#pragma once

#include "../Core/ModuleBase.h"

#include <mq/Plugin.h>

#include <string>
#include <unordered_map>

struct AARow;

class XpBarsModule : public ModuleBase
{
public:
	const char* GetName() const override { return "XPBars"; }

	void OnRenderGUI() override;

private:
	void DrawRow(const AARow& row, float scale, bool showTooltip);

	std::unordered_map<std::string, bool> m_expand;
	std::unordered_map<std::string, bool> m_compact;
};
