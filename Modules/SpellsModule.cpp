#include "SpellsModule.h"

#include "../Core/UiConfig.h"
#include "../Core/ThemeManager.h"
#include "../Core/CharData.h"
#include "../Core/IconHelper.h"
#include "../Core/SpellInfo.h"
#include "../Core/Actions.h"

#include "mq/imgui/Widgets.h"
#include "mq/imgui/ImGuiUtils.h"
#include "imgui/fonts/IconsFontAwesome.h"

#include <cassert>
#include <cmath>

namespace
{
const MQColor kTintReady(255, 255, 255, 255);
const MQColor kTintNotReady(70, 70, 70, 255);
const ImU32 kCooldownShadow = IM_COL32(220, 30, 30, 170);

int CursorSpellId()
{
	if (!pCursorAttachment || !pLocalPC)
	{
		return 0;
	}
	switch (pCursorAttachment->GetType())
	{
	case eCursorAttachment_MemorizeSpell:
		return pLocalPC->GetSpellBook(pCursorAttachment->Index);
	case eCursorAttachment_SpellGem:
		return pLocalPC->GetMemorizedSpell(pCursorAttachment->Index);
	default:
		return 0;
	}
}

bool GemReady(int i)
{
	if (!pLocalPlayer || !pLocalPC || !pDisplay)
	{
		return false;
	}
	if (!GetSpellByID(pLocalPC->GetMemorizedSpell(i)))
	{
		return false;
	}
	return pDisplay->TimeStamp > pLocalPlayer->SpellGemETA[i]
		&& pDisplay->TimeStamp > pLocalPlayer->GetSpellCooldownETA();
}

// Draws a clock-style cooldown shadow: a filled circular pie wedge sized to the
// remaining fraction, whose leading edge sweeps clockwise from 12 o'clock as the
// timer counts down (the icon is revealed behind the sweeping hand).
void DrawRadialCooldown(ImDrawList* drawList, const ImVec2& rectMin, const ImVec2& rectMax, float remaining, ImU32 color)
{
	if (remaining <= 0.0f)
	{
		return;
	}
	if (remaining > 1.0f)
	{
		remaining = 1.0f;
	}

	const ImVec2 center((rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f);
	const float halfW = (rectMax.x - rectMin.x) * 0.5f;
	const float halfH = (rectMax.y - rectMin.y) * 0.5f;
	const float radius = (halfW < halfH ? halfW : halfH) + 3.0f;

	const float pi = 3.14159265358979323846f;
	const float twoPi = 2.0f * pi;
	const float top = -pi * 0.5f;
	const float darkStart = top + (1.0f - remaining) * twoPi;
	const float sweep = remaining * twoPi;

	int segments = static_cast<int>(std::ceil(64.0f * remaining));
	if (segments < 1)
	{
		segments = 1;
	}

	auto edgePoint = [&](float angle) -> ImVec2 {
		return ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
	};

	const bool hadAa = (drawList->Flags & ImDrawListFlags_AntiAliasedFill) != 0;
	drawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
	ImVec2 prev = edgePoint(darkStart);
	for (int s = 1; s <= segments; ++s)
	{
		const float t = static_cast<float>(s) / static_cast<float>(segments);
		const ImVec2 cur = edgePoint(darkStart + sweep * t);
		drawList->AddTriangleFilled(center, prev, cur, color);
		prev = cur;
	}
	if (hadAa)
	{
		drawList->Flags |= ImDrawListFlags_AntiAliasedFill;
	}
}
} // namespace

int SpellsModule::GemCount() const
{
	int count = 8;
	int aaIndex = GetAAIndexByName("Mnemonic Retention");
	if (CAltAbilityData* pAbility = GetAAById(aaIndex))
	{
		if (PlayerHasAAAbility(aaIndex))
		{
			count += pAbility->CurrentRank;
		}
	}
	return count;
}

void SpellsModule::OnInit()
{
	m_picker.InitializeSpells();
}

void SpellsModule::OnPulse()
{
	ProcessDbQueue();

	if (m_picker.m_selectedSpell)
	{
		int id = m_picker.m_selectedSpell->ID;
		const char* name = m_picker.m_selectedSpell->Name;
		if (m_pickerTarget == PickerTarget::LiveGem)
		{
			DoCommandf("/memspell %d \"%s\"", m_pickerSlot + 1, name);
			m_picker.SetOpen(false);
		}
		else if (m_pickerTarget == PickerTarget::SetSlot)
		{
			if (m_pickerSlot >= 0 && m_pickerSlot < static_cast<int>(m_editSet.size()))
			{
				m_editSet[m_pickerSlot] = id;
			}
			m_picker.SetOpen(false);
		}
		m_pickerTarget = PickerTarget::None;
		m_picker.ClearSelection();
	}

	if (!m_picker.m_pickerOpen)
	{
		m_pickerTarget = PickerTarget::None;
	}

	if (m_picker.m_needFilter)
	{
		m_picker.FilterSpells();
	}
}

void SpellsModule::OpenPickerForGem(int gemIndex)
{
	m_pickerTarget = PickerTarget::LiveGem;
	m_pickerSlot = gemIndex;
	m_picker.ClearSelection();
	m_picker.m_filterString.clear();
	m_picker.m_needFilter = true;
	m_picker.SetOpen(true);
}

void SpellsModule::OpenPickerForSetSlot(int slot)
{
	m_pickerTarget = PickerTarget::SetSlot;
	m_pickerSlot = slot;
	m_picker.ClearSelection();
	m_picker.m_filterString.clear();
	m_picker.m_needFilter = true;
	m_picker.SetOpen(true);
}

std::vector<SpellSetRow> SpellsModule::CurrentLoadout() const
{
	std::vector<SpellSetRow> rows;
	if (!pLocalPC)
	{
		return rows;
	}
	int n = GemCount();
	for (int i = 0; i < n; ++i)
	{
		int id = pLocalPC->GetMemorizedSpell(i);
		if (id > 0)
		{
			rows.push_back(SpellSetRow{ i, id });
		}
	}
	return rows;
}

std::vector<SpellSetRow> SpellsModule::EditorRows() const
{
	std::vector<SpellSetRow> rows;
	for (int i = 0; i < static_cast<int>(m_editSet.size()); ++i)
	{
		if (m_editSet[i] > 0)
		{
			rows.push_back(SpellSetRow{ i, m_editSet[i] });
		}
	}
	return rows;
}

void SpellsModule::SeedEditorFromGems()
{
	int n = GemCount();
	m_editSet.assign(n, 0);
	if (!pLocalPC)
	{
		return;
	}
	for (int i = 0; i < n; ++i)
	{
		int id = pLocalPC->GetMemorizedSpell(i);
		m_editSet[i] = (id > 0) ? id : 0;
	}
}

void SpellsModule::LoadSetIntoEditor(const std::string& name)
{
	m_selectedSet = name;
	int n = GemCount();
	m_editSet.assign(n, 0);
	if (!m_ctx.Settings)
	{
		return;
	}
	for (const SpellSetRow& row : m_ctx.Settings->LoadSpellSet(name))
	{
		if (row.gemSlot >= 0 && row.gemSlot < n)
		{
			m_editSet[row.gemSlot] = row.spellId;
		}
	}
}

void SpellsModule::ApplyEditorToGame()
{
	std::vector<myui::SpellSetStep> steps;
	int n = GemCount();
	for (int i = 0; i < n; ++i)
	{
		int id = (i < static_cast<int>(m_editSet.size())) ? m_editSet[i] : 0;
		steps.push_back(myui::SpellSetStep{ i, id });
	}
	myui::StartSpellSetLoad(steps);
}

void SpellsModule::ApplyNamedSetToGame(const std::string& name)
{
	if (!m_ctx.Settings)
	{
		return;
	}
	int n = GemCount();
	std::vector<int> slots(n, 0);
	for (const SpellSetRow& row : m_ctx.Settings->LoadSpellSet(name))
	{
		if (row.gemSlot >= 0 && row.gemSlot < n)
		{
			slots[row.gemSlot] = row.spellId;
		}
	}
	std::vector<myui::SpellSetStep> steps;
	for (int i = 0; i < n; ++i)
	{
		steps.push_back(myui::SpellSetStep{ i, slots[i] });
	}
	myui::StartSpellSetLoad(steps);
}

void SpellsModule::ClearAllGems()
{
	std::vector<myui::SpellSetStep> steps;
	int n = GemCount();
	for (int i = 0; i < n; ++i)
	{
		steps.push_back(myui::SpellSetStep{ i, 0 });
	}
	myui::StartSpellSetLoad(steps);
}

void SpellsModule::RefreshSetNames()
{
	if (m_ctx.Settings)
	{
		m_setNames = m_ctx.Settings->GetSpellSetNames();
	}
}

void SpellsModule::ProcessDbQueue()
{
	if (!m_ctx.Settings)
	{
		return;
	}
	if (!m_dbQueue.empty())
	{
		for (const DbOp& op : m_dbQueue)
		{
			if (op.isDelete)
			{
				m_ctx.Settings->DeleteSpellSet(op.name);
			}
			else
			{
				m_ctx.Settings->SaveSpellSet(op.name, op.rows);
			}
		}
		m_dbQueue.clear();
		m_refreshNames = true;
	}
	if (m_refreshNames)
	{
		m_setNames = m_ctx.Settings->GetSpellSetNames();
		m_refreshNames = false;
	}
}

void SpellsModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !m_ctx.Icons || !pLocalPC)
	{
		return;
	}

	bool vertical = m_ctx.UI->Flag(GetName(), "Vertical", false);
	float gemHeight = m_ctx.UI->Num(GetName(), "GemHeight", 32.0f);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	if (ImGui::Begin("MyUI Spells##MyUISpells", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		const int gemCount = GemCount();
		float scaledGem = gemHeight * w.iconScale;
		int iconPx = static_cast<int>(scaledGem * 0.75f);
		const CXSize gemSize(static_cast<int>(scaledGem * 1.25f), static_cast<int>(scaledGem));

		for (int i = 0; i < gemCount; ++i)
		{
			int spellId = pLocalPC->GetMemorizedSpell(i);

			ImGui::PushID(i);

			EQ_Spell* spell = (spellId > 0) ? GetSpellByID(spellId) : nullptr;
			CSpellGemWnd* gem = pCastSpellWnd ? pCastSpellWnd->SpellSlots[i] : nullptr;

			if (!spell)
			{
				if (CTextureAnimation* holder = m_ctx.Icons->GemHolder())
				{
					holder->SetCurCell(0);
					imgui::DrawTextureAnimation(holder, gemSize);
				}
				else
				{
					ImGui::Dummy(ImVec2(static_cast<float>(scaledGem), static_cast<float>(scaledGem)));
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetItemTooltip("Empty gem - right-click or drop a spell to memorize");
				}
			}
			else
			{
				bool ready = GemReady(i);
				MQColor tintCol = ready ? kTintReady : kTintNotReady;
				MQColor gemTint(255, 255, 255, 255);
				if (gem)
				{
					gemTint = gem->SpellGemTintArray[gem->TintIndex];
				}

				ImGui::BeginGroup();
				float posX = ImGui::GetCursorPosX();
				float posY = ImGui::GetCursorPosY();

				ImGui::SetCursorPos(ImVec2(posX + (gemSize.cx - iconPx) * 0.5f, posY + (gemSize.cy - iconPx) * 0.5f));
				m_ctx.Icons->DrawSpellIcon(spell->SpellIcon, CXSize(iconPx, iconPx), tintCol, MQColor(0, 0, 0, 0));

				ImGui::SetCursorPos(ImVec2(posX, posY));
				if (CTextureAnimation* bg = m_ctx.Icons->GemBackground())
				{
					bg->SetCurCell(0);
					imgui::DrawTextureAnimation(bg, gemSize, gemTint, MQColor(0, 0, 0, 0));
				}
				ImGui::EndGroup();

				ImVec2 rectMin = ImGui::GetItemRectMin();
				ImVec2 rectMax = ImGui::GetItemRectMax();

				if (!ready && pLocalPlayer && pDisplay)
				{
					int remainMs = pLocalPlayer->SpellGemETA[i] - pDisplay->TimeStamp;
					if (remainMs > 0 && spell->RecastTime > 0)
					{
						float ox = (gemSize.cx - iconPx) * 0.5f;
						float oy = (gemSize.cy - iconPx) * 0.5f;
						ImVec2 iconMin(rectMin.x + ox, rectMin.y + oy);
						ImVec2 iconMax(iconMin.x + iconPx, iconMin.y + iconPx);
						float frac = static_cast<float>(remainMs) / static_cast<float>(spell->RecastTime);
						DrawRadialCooldown(ImGui::GetWindowDrawList(), iconMin, iconMax, frac, kCooldownShadow);
					}

					int coolDown = (pLocalPlayer->SpellGemETA[i] - pDisplay->TimeStamp) / 1000;
					if (coolDown > 0 && coolDown < 1801)
					{
						char buf[16];
						sprintf_s(buf, "%d", coolDown);
						ImVec2 textSize = ImGui::CalcTextSize(buf);
						ImVec2 pos((rectMin.x + rectMax.x - textSize.x) * 0.5f,
							(rectMin.y + rectMax.y - textSize.y) * 0.5f);
						ImGui::GetWindowDrawList()->AddText(pos, IM_COL32(0, 230, 230, 255), buf);
					}
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.3f, 1.0f), "%s", spell->Name);
					myui::RenderSpellEffects(spellId);
					ImGui::EndTooltip();
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !CursorSpellId())
					{
						DoCommandf("/cast %d", i + 1);
					}
				}
			}

			if (!spell && CursorSpellId() && ImGui::IsItemHovered()
				&& ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				if (pCastSpellWnd && pCastSpellWnd->SpellSlots[i])
				{
					SendWndClick2(pCastSpellWnd->SpellSlots[i], "leftmouseup");
				}
			}

			if (spell)
			{
				if (ImGui::BeginPopupContextItem("##GemCtx"))
				{
					if (ImGui::MenuItem("Replace Spell..."))
					{
						OpenPickerForGem(i);
					}
					if (ImGui::MenuItem("Clear Gem"))
					{
						if (pCastSpellWnd)
						{
							pCastSpellWnd->ForgetMemorizedSpell(i);
						}
					}
					ImGui::EndPopup();
				}
			}
			else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				OpenPickerForGem(i);
			}

			ImGui::PopID();

			if (!vertical)
			{
				ImGui::SameLine(0.0f, 2.0f);
			}
		}

		DrawOptionsButton();

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}

	m_picker.DrawSpellPicker();
	DrawSpellSetManager();
}

void SpellsModule::DrawOptionsButton()
{
	float gemHeight = m_ctx.UI->Num(GetName(), "GemHeight", 32.0f) * m_ctx.UI->Window(GetName()).iconScale;

	ImGui::PushID("OptionsButton");

	if (ImGui::Button(ICON_FA_COG, ImVec2(gemHeight, gemHeight)))
	{
		m_managerOpen = !m_managerOpen;
		if (m_managerOpen)
		{
			RefreshSetNames();
			if (m_editSet.empty())
			{
				SeedEditorFromGems();
			}
		}
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		RefreshSetNames();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("Left-click: spell sets | Right-click: quick save / load");
	}

	DrawQuickSetMenu();

	ImGui::PopID();
}

void SpellsModule::DrawQuickSetMenu()
{
	if (!ImGui::BeginPopupContextItem("##quicksets"))
	{
		return;
	}

	ImGui::TextDisabled("Save current gems as:");
	ImGui::SetNextItemWidth(150.0f);
	ImGui::InputTextWithHint("##qsname", "set name", m_newNameBuf, sizeof(m_newNameBuf));
	ImGui::SameLine();
	bool canSave = m_newNameBuf[0] != 0;
	ImGui::BeginDisabled(!canSave);
	if (ImGui::Button("Save"))
	{
		m_dbQueue.push_back(DbOp{ false, m_newNameBuf, CurrentLoadout() });
		m_newNameBuf[0] = 0;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::TextDisabled("Load set:");
	bool loading = myui::SpellSetLoadActive();
	ImGui::BeginDisabled(loading);
	if (m_setNames.empty())
	{
		ImGui::TextDisabled("(no saved sets)");
	}
	for (const std::string& name : m_setNames)
	{
		if (ImGui::Selectable(name.c_str()))
		{
			ApplyNamedSetToGame(name);
			ImGui::CloseCurrentPopup();
		}
	}

	ImGui::Separator();
	if (ImGui::Button(ICON_FA_TRASH_O " Clear All Gems"))
	{
		ClearAllGems();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();

	ImGui::EndPopup();
}

void SpellsModule::DrawSpellSetManager()
{
	if (!m_managerOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(470.0f, 410.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Spell Sets##MyUISpellSets", &m_managerOpen))
	{
		ImGui::TextDisabled("Spell Sets");
		ImGui::SameLine();
		mq::imgui::HelpMarker(
			"Named gem loadouts, saved per character.\n\n"
			"Pick a set on the left to edit it. Click a slot's icon or name to choose a spell, "
			"or the trash button to clear it - editing here does NOT change your live gems.\n\n"
			"Save / Update store the set. 'Load to Game' memorizes it onto your gems "
			"(it sits, mems every spell, then stands; slots not in the set are cleared).");

		// Top: name + Save / Update / Delete
		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputTextWithHint("##setname", "set name", m_newNameBuf, sizeof(m_newNameBuf));
		ImGui::SameLine();
		bool canSave = m_newNameBuf[0] != 0;
		ImGui::BeginDisabled(!canSave);
		if (ImGui::Button("Save"))
		{
			m_dbQueue.push_back(DbOp{ false, m_newNameBuf, EditorRows() });
			m_selectedSet = m_newNameBuf;
			m_newNameBuf[0] = 0;
		}
		ImGui::EndDisabled();
		ImGui::SetItemTooltip("Save the edited loadout under the typed name (overwrites a set with that name).");

		if (!m_selectedSet.empty())
		{
			ImGui::SameLine();
			if (ImGui::Button("Update"))
			{
				m_dbQueue.push_back(DbOp{ false, m_selectedSet, EditorRows() });
			}
			ImGui::SetItemTooltip("Overwrite the selected set with the current edits.");

			ImGui::SameLine();
			if (!m_confirmDelete)
			{
				if (ImGui::Button("Delete"))
				{
					m_confirmDelete = true;
				}
				ImGui::SetItemTooltip("Delete the selected set.");
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
				if (ImGui::Button("Confirm"))
				{
					m_dbQueue.push_back(DbOp{ true, m_selectedSet, {} });
					m_selectedSet.clear();
					m_confirmDelete = false;
				}
				ImGui::PopStyleColor();
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					m_confirmDelete = false;
				}
			}
		}

		ImGui::Separator();

		// Middle: set list (left) | slot table (right)
		float bottomH = ImGui::GetFrameHeightWithSpacing() + 4.0f;
		ImGui::BeginChild("##setmid", ImVec2(0.0f, -bottomH));

		ImGui::BeginChild("##setlist", ImVec2(130.0f, 0.0f), ImGuiChildFlags_Borders);
		ImGui::TextDisabled("Saved Sets");
		ImGui::Separator();
		for (const std::string& name : m_setNames)
		{
			if (ImGui::Selectable(name.c_str(), name == m_selectedSet))
			{
				LoadSetIntoEditor(name);
				m_confirmDelete = false;
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##setedit", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
		if (ImGui::BeginTable("##setslots", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH))
		{
			ImGui::TableSetupColumn("Slot");
			ImGui::TableSetupColumn("Icon");
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("##clear");
			ImGui::TableHeadersRow();

			for (int i = 0; i < static_cast<int>(m_editSet.size()); ++i)
			{
				ImGui::PushID(i);
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("%d", i + 1);

				int id = m_editSet[i];
				EQ_Spell* spell = (id > 0) ? GetSpellByID(id) : nullptr;

				ImGui::TableNextColumn();
				if (spell && m_ctx.Icons)
				{
					m_ctx.Icons->DrawSpellIcon(spell->SpellIcon, CXSize(20, 20),
						MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));
				}
				else
				{
					ImGui::Dummy(ImVec2(20.0f, 20.0f));
				}
				if (ImGui::IsItemClicked())
				{
					OpenPickerForSetSlot(i);
				}
				ImGui::SetItemTooltip("Click to choose a spell for slot %d.", i + 1);

				ImGui::TableNextColumn();
				const char* label = spell ? spell->Name : "(empty)";
				if (ImGui::Selectable(label))
				{
					OpenPickerForSetSlot(i);
				}
				ImGui::SetItemTooltip("Click to choose a spell for slot %d.", i + 1);

				ImGui::TableNextColumn();
				if (id > 0)
				{
					if (ImGui::SmallButton(ICON_FA_TRASH_O))
					{
						m_editSet[i] = 0;
					}
					ImGui::SetItemTooltip("Clear this slot.");
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::EndChild();

		ImGui::EndChild();

		// Bottom: New | From Gems | Load to Game
		if (ImGui::Button("New"))
		{
			m_selectedSet.clear();
			m_editSet.assign(GemCount(), 0);
			m_confirmDelete = false;
		}
		ImGui::SetItemTooltip("Start a new, empty loadout.");
		ImGui::SameLine();
		if (ImGui::Button("From Gems"))
		{
			SeedEditorFromGems();
			m_confirmDelete = false;
		}
		ImGui::SetItemTooltip("Fill the editor from your currently memorized gems.");
		ImGui::SameLine();
		if (myui::SpellSetLoadActive())
		{
			ImGui::TextDisabled("Loading...");
		}
		else
		{
			if (ImGui::Button("Load to Game"))
			{
				ApplyEditorToGame();
			}
			ImGui::SetItemTooltip("Memorize this loadout onto your gems (clears slots not in the set).");
		}
	}
	ImGui::End();
}
