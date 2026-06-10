#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/InventoryData.h"

#include <set>

class MyInventoryModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Inventory"; }

	void OnRenderGUI() override;

private:
	void DrawStatsTab();
	void DrawInventoryStats();
	void DrawInventoryResists();
	void DrawInventoryFooter(bool& visible);
	void DrawPaperdoll(float cellSize, bool showBackground);
	void DrawBags(float cellSize, bool showBackground);
	void DrawSwapMenu(int wornSlot);
	void DrawContainerWindows(float cellSize, bool showBackground);

	std::set<int> m_openContainers;
	myui::CoinPicker m_coin;
	int m_tab = 0;
};
