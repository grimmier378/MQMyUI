#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/CharData.h"

#include <mq/Plugin.h>

#include <string>
#include <vector>

class SongsModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Songs"; }

	void OnPulse() override;
	void OnRenderGUI() override;

private:
	void DrawTableView(float pulse);
	void DrawIconView(float pulse);
	void DrawIconLineupRow(const char* rowName, const std::vector<BuffInfo>& songs, float pulse);
};
