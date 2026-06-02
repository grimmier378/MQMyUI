#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/CharData.h"

#include <mq/Plugin.h>

#include <string>
#include <vector>

class BuffsModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Buffs"; }

	void OnPulse() override;
	void OnRenderGUI() override;

private:
	void DrawTableView(float pulse);
	void DrawIconView(float pulse);
	void DrawIconLineupRow(const char* rowName, const std::vector<BuffInfo>& buffs, float pulse, const std::string& targetChar);
	void BuffContextMenu(const BuffInfo& buff, const std::string& targetChar);

	std::string m_selectedChar;
};
