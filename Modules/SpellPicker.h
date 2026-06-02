#pragma once

#include "eqlib/game/Spells.h"
#include <mq/Plugin.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Adapted from MQGrimGUI (originally Aquietone's Lua AbilityPicker).
class SpellPicker
{
public:
	struct SpellData
	{
		int ID = 0;
		int Level = 0;
		int IconID = 0;
		int RankNum = 0;
		int SpellBookIndex = 0;
		char* Name = nullptr;
		const char* Category = nullptr;
		const char* SubCategory = nullptr;
	};

	bool m_pickerOpen = false;
	bool m_needFilter = false;

	std::shared_ptr<SpellData> m_selectedSpell;
	std::vector<SpellData>     m_mySpells;

	CTextureAnimation* m_pSpellIcon = nullptr;

	std::string m_filterString;

	SpellPicker();
	~SpellPicker();

	void DrawSpellPicker();
	void DrawSpellTree();
	void InitializeSpells();
	void FilterSpells();

	void SetOpen(bool open) { m_pickerOpen = open; }
	void ClearSelection() { m_selectedSpell.reset(); }

private:
	void PopulateSpellData();

	std::unordered_map<const char*, std::unordered_map<const char*, std::vector<SpellData>>> m_categorized;
};
