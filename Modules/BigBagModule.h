#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/InventoryData.h"

#include <map>
#include <string>
#include <vector>

class BigBagModule : public ModuleBase
{
public:
	const char* GetName() const override { return "BigBag"; }

	void OnRenderGUI() override;

private:
	void DrawHeader(float iconSize);
	void DrawControls();
	void DrawItemGrid(const std::vector<myui::ItemRef>& items, float cellSize, bool showBackground);
	void DrawDetails();

	bool PassesNameFilter(const myui::ItemRef& ref) const;
	bool PassesPreset(const myui::ItemRef& ref) const;
	bool PassesUseableFilter(const myui::ItemRef& ref) const;
	void SortItems(std::vector<myui::ItemRef>& items) const;

	char m_filter[128] = { 0 };
	int  m_preset = 0;
	int  m_tab = 0;
	std::map<std::string, bool> m_tradeChecked;
	myui::CoinPicker m_coin;
};
