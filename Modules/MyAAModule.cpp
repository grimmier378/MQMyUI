#include "MyAAModule.h"

#include "../Core/UiConfig.h"
#include "../Core/CharData.h"
#include "../Core/ChatBridge.h"
#include "../Core/Widgets.h"

#include "mq/base/String.h"

#include <string>
#include <vector>

namespace
{
const char* kTypeNames[] = { "", "General", "Archetype", "Class", "Special", "Focus" };
const char* kAAListNames[] = { "AAW_GeneralList", "AAW_ArchList", "AAW_ClassList", "AAW_SpecialList" };

int FindAARow(const std::string& name, CListWnd** outList)
{
	*outList = nullptr;
	for (const char* listName : kAAListNames)
	{
		CXWnd* child = pAAWnd->GetChildItem(listName);
		if (!child)
		{
			continue;
		}

		CListWnd* list = static_cast<CListWnd*>(child);
		int count = list->ItemsArray.GetLength();
		for (int r = 0; r < count; ++r)
		{
			CXStr text = list->GetItemText(r, 0);
			if (ci_equals(text.c_str(), name.c_str()))
			{
				*outList = list;
				return r;
			}
		}
	}
	return -1;
}

bool SelectAAInList(const std::string& name)
{
	CListWnd* list = nullptr;
	int row = FindAARow(name, &list);
	if (row < 0 || !list)
	{
		return false;
	}

	list->SetCurSel(row);
	list->EnsureVisible(row);
	CXPoint pt = list->GetItemRect(row, 0).CenterPoint();
	list->HandleLButtonDown(pt, 0);
	list->HandleLButtonUp(pt, 0);
	return true;
}

CTabWnd* GetAATabs()
{
	if (!pAAWnd)
	{
		return nullptr;
	}
	CXWnd* tabChild = pAAWnd->GetChildItem("AAW_Subwindows");
	if (!tabChild || tabChild->GetType() != UI_TabBox)
	{
		return nullptr;
	}
	return static_cast<CTabWnd*>(tabChild);
}

void SyncNativeTab(int type)
{
	CTabWnd* tabs = GetAATabs();
	if (!tabs)
	{
		return;
	}
	int nativeTab = (type >= 1 && type <= 4) ? (type - 1) : 3;
	if (nativeTab >= 0 && nativeTab < tabs->PageArray.GetCount() && tabs->GetCurrentTabIndex() != nativeTab)
	{
		tabs->SetPage(nativeTab);
	}
}

bool ClickAAButton(const std::string& name, const char* buttonId)
{
	if (!pAAWnd)
	{
		return false;
	}

	bool selected = SelectAAInList(name);
	if (!selected)
	{
		CTabWnd* tabs = GetAATabs();
		if (tabs)
		{
			int count = tabs->PageArray.GetCount();
			for (int t = 0; t < count && !selected; ++t)
			{
				tabs->SetPage(t);
				selected = SelectAAInList(name);
			}
		}
	}
	if (!selected)
	{
		myui::ChatOutf("\ar[MyUI AA]\ax could not find '%s' in the AA window lists.", name.c_str());
		return false;
	}

	CXWnd* btn = pAAWnd->GetChildItem(buttonId);
	if (!btn)
	{
		myui::ChatOutf("\ar[MyUI AA]\ax button '%s' not found.", buttonId);
		return false;
	}
	return SendWndClick2(btn, "leftmouseup");
}
} // namespace

void MyAAModule::DrawTable(int type, float height)
{
	bool trainableOnly = m_ctx.UI->Flag(GetName(), "TrainableOnly", false);
	bool affordableOnly = m_ctx.UI->Flag(GetName(), "AffordableOnly", false);
	bool confirmPurchase = m_ctx.UI->Flag(GetName(), "ConfirmPurchase", false);
	bool showHotkey = m_ctx.UI->Flag(GetName(), "ShowHotkeyButton", true);

	std::vector<const AAEntry*> list = m_aaData.ByType(type);
	int avail = m_aaData.Available();

	ImGuiTableFlags tflags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg
		| ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

	if (ImGui::BeginTable("##aatable", 6, tflags, ImVec2(0.0f, height)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 52.0f);
		ImGui::TableSetupColumn("Cost", ImGuiTableColumnFlags_WidthFixed, 42.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Train", ImGuiTableColumnFlags_WidthFixed, 52.0f);
		ImGui::TableSetupColumn("Hotkey", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableHeadersRow();

		for (const AAEntry* e : list)
		{
			bool affordable = e->canTrain && e->nextCost <= avail;
			if (trainableOnly && !e->canTrain)
			{
				continue;
			}
			if (affordableOnly && !affordable)
			{
				continue;
			}
			if (ci_find_substr(e->name, m_search) == -1)
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::PushID(e->groupId);

			ImGui::TableNextColumn();
			bool selected = (e->groupId == m_selected);
			if (ImGui::Selectable(e->name.c_str(), selected,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			{
				m_selected = e->groupId;
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)
				&& !e->passive && pSpellDisplayManager)
			{
				pSpellDisplayManager->ShowSpell(e->spellId, true, true, SpellDisplayType_SpellBookWnd);
			}

			ImGui::TableNextColumn();
			ImGui::Text("%d/%d", e->curRank, e->maxRank);

			ImGui::TableNextColumn();
			if (e->curRank >= e->maxRank)
			{
				ImGui::TextDisabled("-");
			}
			else
			{
				ImGui::TextColored(affordable ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
					"%d", e->nextCost);
			}

			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", e->passive ? "Passive" : "Active");

			ImGui::TableNextColumn();
			if (e->canTrain)
			{
				ImGui::BeginDisabled(!affordable);
				if (myui::StyledSmallButton("Train"))
				{
					if (confirmPurchase)
					{
						m_confirmGroup = e->groupId;
						m_openConfirm = true;
					}
					else
					{
						m_pendingBuy = e->groupId;
					}
				}
				ImGui::EndDisabled();
			}

			ImGui::TableNextColumn();
			if (showHotkey && !e->passive && e->curRank > 0)
			{
				if (myui::StyledSmallButton("Hotkey"))
				{
					m_pendingHotkey = e->groupId;
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void MyAAModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame())
	{
		return;
	}

	m_aaData.Update();

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	ImGui::SetNextWindowSize(ImVec2(460, 560), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("AA##MyUIAA", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Available: %d", m_aaData.Available());
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Spent: %d", m_aaData.Spent());
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Total: %d", m_aaData.Total());

		int allCur = 0;
		int allMax = 0;
		m_aaData.AllRanks(allCur, allMax);
		ImGui::TextDisabled("Ranks:");
		for (int t = 1; t <= 5; ++t)
		{
			int c = 0;
			int m = 0;
			m_aaData.CategoryRanks(t, c, m);
			ImGui::SameLine();
			ImGui::TextDisabled("%s %d/%d", kTypeNames[t], c, m);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("All %d/%d", allCur, allMax);

		ImGui::SetNextItemWidth(160.0f * w.textScale);
		ImGui::InputTextWithHint("##search", "search...", m_search, sizeof(m_search));

		bool trainableOnly = m_ctx.UI->Flag(GetName(), "TrainableOnly", false);
		bool affordableOnly = m_ctx.UI->Flag(GetName(), "AffordableOnly", false);
		ImGui::SameLine();
		if (myui::StyledCheckbox("Trainable", &trainableOnly))
		{
			m_ctx.UI->SetFlag(GetName(), "TrainableOnly", trainableOnly);
		}
		ImGui::SameLine();
		if (myui::StyledCheckbox("Affordable", &affordableOnly))
		{
			m_ctx.UI->SetFlag(GetName(), "AffordableOnly", affordableOnly);
		}

		ImGui::Separator();

		float descH = ImGui::GetTextLineHeightWithSpacing() * 7.0f;
		float tableH = ImGui::GetContentRegionAvail().y - descH - ImGui::GetStyle().ItemSpacing.y;
		if (tableH < 80.0f)
		{
			tableH = 80.0f;
		}

		std::string tabLabels[5];
		const char* tabPtrs[5];
		for (int t = 1; t <= 5; ++t)
		{
			int c = 0;
			int m = 0;
			m_aaData.CategoryRanks(t, c, m);
			tabLabels[t - 1] = fmt::format("{} ({}/{})###tab{}", kTypeNames[t], c, m, t);
			tabPtrs[t - 1] = tabLabels[t - 1].c_str();
		}
		int tabSel = myui::PillTabBar("##MyAATabs", tabPtrs, 5, m_activeType - 1);
		m_activeType = tabSel + 1;
		DrawTable(m_activeType, tableH);

		SyncNativeTab(m_activeType);

		ImGui::BeginChild("##aadesc", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
		const AAEntry* sel = m_aaData.Find(m_selected);
		if (sel)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s  (%d/%d)",
				sel->name.c_str(), sel->curRank, sel->maxRank);
			if (sel->passive)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("Passive");
			}
			else
			{
				ImGui::SameLine();
				if (m_aaData.Ready(sel->groupId))
				{
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Ready");
				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "On Cooldown");
				}
				if (sel->reuseSecs > 0)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("Reuse %d:%02d", sel->reuseSecs / 60, sel->reuseSecs % 60);
				}
			}
			ImGui::Separator();
			std::string desc = m_aaData.Description(sel->groupId);
			ImGui::TextWrapped("%s", desc.c_str());
		}
		else
		{
			ImGui::TextDisabled("Select an AA to see its description.");
		}
		ImGui::EndChild();

		if (m_openConfirm)
		{
			ImGui::OpenPopup("Confirm Purchase##myaa");
			m_openConfirm = false;
		}
		if (ImGui::BeginPopupModal("Confirm Purchase##myaa", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const AAEntry* ce = m_aaData.Find(m_confirmGroup);
			if (ce)
			{
				ImGui::Text("Train %s rank %d for %d AA?", ce->name.c_str(), ce->curRank + 1, ce->nextCost);
				if (myui::StyledButton("Train"))
				{
					m_pendingBuy = m_confirmGroup;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (myui::StyledButton("Cancel"))
				{
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}
}

void MyAAModule::OnPulse()
{
	if (m_pendingBuy >= 0)
	{
		const AAEntry* e = m_aaData.Find(m_pendingBuy);
		if (e)
		{
			ClickAAButton(e->name, "AAW_TrainButton");
		}
		m_pendingBuy = -1;
	}

	if (m_pendingHotkey >= 0)
	{
		const AAEntry* e = m_aaData.Find(m_pendingHotkey);
		if (e)
		{
			ClickAAButton(e->name, "AAW_HotButton");
		}
		m_pendingHotkey = -1;
	}
}
