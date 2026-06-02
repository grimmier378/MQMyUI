#pragma once

#include "../Core/ModuleBase.h"

class ToolbarModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Toolbar"; }

	void OnRenderGUI() override;

private:
	bool IconButton(int iconId, float size, const char* tooltip);
};
