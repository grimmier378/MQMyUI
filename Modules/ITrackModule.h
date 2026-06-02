#pragma once

#include "../Core/ModuleBase.h"

#include <string>

class ITrackModule : public ModuleBase
{
public:
	const char* GetName() const override { return "iTrack"; }

	void OnRenderGUI() override;

private:
	void DrawItemList();
	void DrawItemDetails();

	char m_input[128] = { 0 };
	std::string m_selected;
};
