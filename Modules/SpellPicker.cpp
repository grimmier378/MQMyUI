// Spell Picker — adapted from MQGrimGUI (original Lua AbilityPicker by Aquietone).
#include "SpellPicker.h"

#include "../Core/SpellInfo.h"
#include "../Core/Widgets.h"

#include <mq/imgui/Widgets.h>

#include <algorithm>

void SpellPicker::DrawSpellTree()
{
	if (ImGui::TreeNode("Spells"))
	{
		for (const auto& [categoryName, subCategories] : m_categorized)
		{
			if (ImGui::TreeNode(categoryName))
			{
				for (const auto& [subCategoryName, spells] : subCategories)
				{
					if (ImGui::TreeNode(subCategoryName))
					{
						for (const auto& spell : spells)
						{
							ImGui::PushID(spell.ID);
							ImGui::BeginGroup();

							if (m_pSpellIcon)
							{
								m_pSpellIcon->SetCurCell(spell.IconID);
								imgui::DrawTextureAnimation(m_pSpellIcon, CXSize(20, 20));
								ImGui::SameLine();
							}

							if (ImGui::Selectable(spell.Name))
							{
								m_selectedSpell = std::make_shared<SpellData>(spell);
							}

							ImGui::SameLine();
							ImGui::Text("Level: %d", spell.Level);

							ImGui::EndGroup();
							ImGui::PopID();

							if (ImGui::IsItemHovered())
							{
								ImGui::BeginTooltip();
								ImGui::Text("Name: %s", spell.Name);
								ImGui::Text("Level: %d", spell.Level);
								ImGui::Text("Rank: %d", spell.RankNum);
								myui::RenderSpellEffects(spell.ID);
								ImGui::EndTooltip();
							}
						}
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}

void SpellPicker::InitializeSpells()
{
	if (!m_pSpellIcon && pSidlMgr)
	{
		if (CTextureAnimation* temp = pSidlMgr->FindAnimation("A_SpellGems"))
		{
			m_pSpellIcon = new CTextureAnimation();
			*m_pSpellIcon = *temp;
		}
	}

	m_mySpells.clear();
	PopulateSpellData();
}

void SpellPicker::PopulateSpellData()
{
	if (!pLocalPC)
	{
		return;
	}

	for (int i = 0; i < NUM_BOOK_SLOTS; ++i)
	{
		int spellId = pLocalPC->GetSpellBook(i);
		if (spellId == -1)
		{
			continue;
		}

		EQ_Spell* pSpell = mq::GetSpellByID(spellId);
		if (!pSpell)
		{
			continue;
		}

		SpellData spellData{};
		spellData.ID = spellId;
		spellData.Name = pSpell->Name;
		spellData.RankNum = pSpell->SpellRank ? pSpell->SpellRank : 0;
		spellData.Level = pSpell->GetSpellLevelNeeded(pLocalPC->GetClass());
		spellData.IconID = pSpell->SpellIcon;
		spellData.SpellBookIndex = i;
		spellData.Category = pCDBStr->GetString(pSpell->Category, eSpellCategory)
			? pCDBStr->GetString(pSpell->Category, eSpellCategory) : "NONE";
		spellData.SubCategory = pCDBStr->GetString(pSpell->Subcategory, eSpellCategory)
			? pCDBStr->GetString(pSpell->Subcategory, eSpellCategory) : "NONE";

		m_mySpells.push_back(spellData);
	}

	std::sort(m_mySpells.begin(), m_mySpells.end(), [](const SpellData& a, const SpellData& b)
		{
			if (a.Category != b.Category)
			{
				return a.Category < b.Category;
			}
			if (a.SubCategory != b.SubCategory)
			{
				return a.SubCategory < b.SubCategory;
			}
			return a.Level > b.Level;
		});

	RebuildCategorized(false);
}

void SpellPicker::RebuildCategorized(bool applyFilter)
{
	m_categorized.clear();
	for (const auto& spell : m_mySpells)
	{
		if (applyFilter
			&& !m_filterString.empty()
			&& mq::ci_find_substr(spell.Name, m_filterString) == -1
			&& mq::ci_find_substr(spell.Category, m_filterString) == -1
			&& mq::ci_find_substr(spell.SubCategory, m_filterString) == -1)
		{
			continue;
		}

		m_categorized[spell.Category][spell.SubCategory].push_back(spell);
	}
}

void SpellPicker::FilterSpells()
{
	if (!m_needFilter)
	{
		RebuildCategorized(false);
		return;
	}

	RebuildCategorized(true);
	m_needFilter = false;
}

void SpellPicker::DrawSpellPicker()
{
	if (!m_pickerOpen)
	{
		return;
	}

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - 300.0f) * 0.5f, vp->WorkPos.y + (vp->WorkSize.y - 400.0f) * 0.5f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Spell Picker##MyUISpellPicker", &m_pickerOpen, ImGuiWindowFlags_NoDocking))
	{
		// Live search field — left as a plain InputText (per-keystroke filtering), not
		// the commit-on-accept StyledEditField.
		char searchBuf[128];
		strncpy_s(searchBuf, sizeof(searchBuf), m_filterString.c_str(), _TRUNCATE);
		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::InputTextWithHint("##SpellSearch", "search", searchBuf, sizeof(searchBuf)))
		{
			m_filterString = searchBuf;
			m_needFilter = true;
		}

		ImGui::SameLine();
		if (myui::StyledButton("Refresh##SpellPicker"))
		{
			InitializeSpells();
		}

		DrawSpellTree();
	}
	ImGui::End();
}

SpellPicker::SpellPicker()
{
	InitializeSpells();
}

SpellPicker::~SpellPicker()
{
	if (m_pSpellIcon)
	{
		delete m_pSpellIcon;
		m_pSpellIcon = nullptr;
	}
}
