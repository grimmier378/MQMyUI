#include "BigBagModule.h"

#include "../Core/UiConfig.h"
#include "../Core/HotkeyCombo.h"
#include "../Core/IconHelper.h"
#include "../Core/CharData.h"
#include "../Core/Actions.h"
#include "../Core/UiHelpers.h"

#include <algorithm>

namespace
{
const char* kPresetNames[] = { "All", "Weapons", "Ranged", "Charm", "Ears", "Face", "Neck", "Rings" };
} // namespace

void BigBagModule::OnRenderGUI()
{
	UiConfig* ui = m_ctx.UI;
	if (!ui)
	{
		return;
	}

	WindowConfig& w = ui->Window(GetName());

	if (myui::CheckToggleCombo(myui::ReadCombo(w)))
	{
		w.visible = !w.visible;
		ui->PersistVisibility(GetName());
	}

	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !pLocalPlayer)
	{
		return;
	}

	ImGuiWindowFlags flags = 0;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	float cell = ui->Num(GetName(), "ItemSize", 40.0f) * w.iconScale;
	bool showBackground = ui->Flag(GetName(), "ShowSlotBackground", true);
	bool highlight = ui->Flag(GetName(), "DimUnusable", true);
	float iconSize = 20.0f * w.iconScale;

	ImGui::SetNextWindowSize(ImVec2(520.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI BigBag##MyUIBigBag", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		DrawHeader(iconSize);
		DrawControls();
		ImGui::Separator();

		if (ImGui::BeginTabBar("##BigBagTabs", ImGuiTabBarFlags_Reorderable))
		{
			if (ImGui::BeginTabItem("Items"))
			{
				ImGui::SetNextItemWidth(140.0f);
				if (ImGui::BeginCombo("Category", kPresetNames[m_preset]))
				{
					for (int i = 0; i < IM_ARRAYSIZE(kPresetNames); ++i)
					{
						if (ImGui::Selectable(kPresetNames[i], i == m_preset))
						{
							m_preset = i;
						}
					}
					ImGui::EndCombo();
				}

				std::vector<myui::ItemRef> items;
				for (myui::ItemRef& ref : myui::GetBagContents())
				{
					if (PassesNameFilter(ref) && PassesPreset(ref))
					{
						items.push_back(ref);
					}
				}
				SortItems(items);
				DrawItemGrid(items, cell, showBackground, highlight);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Clickies"))
			{
				std::vector<myui::ItemRef> items;
				for (myui::ItemRef& ref : myui::GetBagContents())
				{
					if (myui::IsClicky(ref) && PassesNameFilter(ref))
					{
						items.push_back(ref);
					}
				}
				SortItems(items);
				DrawItemGrid(items, cell, showBackground, highlight);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Augments"))
			{
				std::vector<myui::ItemRef> items;
				for (myui::ItemRef& ref : myui::GetBagContents())
				{
					if (myui::IsAugment(ref) && PassesNameFilter(ref))
					{
						items.push_back(ref);
					}
				}
				SortItems(items);
				DrawItemGrid(items, cell, showBackground, highlight);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Bank"))
			{
				std::vector<myui::ItemRef> items;
				for (myui::ItemRef& ref : myui::GetBank())
				{
					if (PassesNameFilter(ref))
					{
						items.push_back(ref);
					}
				}
				SortItems(items);
				DrawItemGrid(items, cell, showBackground, highlight);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Details"))
			{
				DrawDetails();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::PopFont();
	}
	ImGui::End();

	myui::DrawCoinQuantityWindow(m_coin);

	if (!w.visible)
	{
		ui->PersistVisibility(GetName());
	}
}

void BigBagModule::DrawHeader(float iconSize)
{
	int free = myui::GetFreeSlots();
	int warn = static_cast<int>(m_ctx.UI->Num(GetName(), "MinSlotsWarn", 3.0f));
	ImColor freeColor = free > warn ? ImColor(90, 220, 0) : ImColor(255, 90, 0);

	ImGui::Text("Free Slots:");
	ImGui::SameLine();
	ImGui::TextColored(freeColor, "%d", free);
	ImGui::SameLine();
	myui::DrawCurrencyRow(m_ctx.Icons, iconSize, m_coin);
}

void BigBagModule::DrawControls()
{
	UiConfig* ui = m_ctx.UI;

	ImGui::SetNextItemWidth(200.0f);
	ImGui::InputTextWithHint("##filter", "Search...", m_filter, sizeof(m_filter));
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		m_filter[0] = 0;
	}

	bool sortName = ui->Flag(GetName(), "SortName", true);
	if (ImGui::Checkbox("Name", &sortName))
	{
		ui->SetFlag(GetName(), "SortName", sortName);
	}
	ImGui::SameLine();
	bool sortStack = ui->Flag(GetName(), "SortStack", true);
	if (ImGui::Checkbox("Stack", &sortStack))
	{
		ui->SetFlag(GetName(), "SortStack", sortStack);
	}
	ImGui::SameLine();
	bool sortType = ui->Flag(GetName(), "SortType", true);
	if (ImGui::Checkbox("Type", &sortType))
	{
		ui->SetFlag(GetName(), "SortType", sortType);
	}
	ImGui::SameLine();
	bool highlight = ui->Flag(GetName(), "DimUnusable", true);
	if (ImGui::Checkbox("Dim Unusable", &highlight))
	{
		ui->SetFlag(GetName(), "DimUnusable", highlight);
	}

	myui::DrawDropZones();
}

void BigBagModule::DrawItemGrid(const std::vector<myui::ItemRef>& items, float cellSize, bool showBackground, bool highlight)
{
	ImGui::BeginChild("##grid");
	float avail = ImGui::GetContentRegionAvail().x;
	int perRow = myui::ColumnsForWidth(avail, cellSize);

	for (size_t i = 0; i < items.size(); ++i)
	{
		if (i % perRow != 0)
		{
			ImGui::SameLine();
		}
		ImGui::PushID(static_cast<int>(i));
		myui::DrawItemOptions opts;
		opts.size = cellSize;
		opts.showBackground = showBackground;
		opts.highlightUseable = highlight;
		myui::DrawItemIcon(m_ctx.Icons, items[i], opts);
		ImGui::PopID();
	}
	ImGui::EndChild();
}

void BigBagModule::DrawDetails()
{
	if (ImGui::Button("Trade Selected Items"))
	{
		std::vector<std::string> names;
		for (auto& [name, checked] : m_tradeChecked)
		{
			if (checked)
			{
				names.push_back(name);
			}
		}
		if (!names.empty())
		{
			myui::StartBulkTrade(names);
			for (auto& [name, checked] : m_tradeChecked)
			{
				checked = false;
			}
		}
	}
	ImGui::SameLine();
	bool checkAll = ImGui::Button("Check All");

	ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable;

	if (ImGui::BeginTable("##Details", 9, tableFlags))
	{
		ImGui::TableSetupColumn("Trade", ImGuiTableColumnFlags_WidthFixed, 30.0f);
		ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 24.0f);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Tribute", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Worn EFX", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Clicky", ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn("Charges", ImGuiTableColumnFlags_WidthFixed, 55.0f);
		ImGui::TableSetupColumn("Augment", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		std::vector<myui::ItemRef> items = myui::GetBagContents();
		SortItems(items);

		for (size_t i = 0; i < items.size(); ++i)
		{
			myui::ItemRef& ref = items[i];
			if (!PassesNameFilter(ref))
			{
				continue;
			}
			ItemDefinition* def = ref.item->GetItemDefinition();
			if (!def)
			{
				continue;
			}

			ImGui::PushID(static_cast<int>(i));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			std::string name = ref.name();
			if (def->IsDroppable)
			{
				bool& checked = m_tradeChecked[name];
				if (checkAll)
				{
					checked = true;
				}
				ImGui::Checkbox("##t", &checked);
			}
			else
			{
				ImGui::TextColored(ImColor(130, 102, 26), "NA");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("This item is NO TRADE");
				}
			}

			ImGui::TableNextColumn();
			myui::DrawItemOptions opts;
			opts.size = 20.0f;
			opts.showBackground = false;
			myui::DrawItemIcon(m_ctx.Icons, ref, opts);

			ImGui::TableNextColumn();
			ImGui::TextColored(ImColor(0, 255, 255), "%s", name.c_str());

			ImGui::TableNextColumn();
			ImGui::TextColored(ImColor(0, 255, 128), "%.2f pp", def->Cost / 1000.0f);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("-");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(def->SpellData.GetSpellId(ItemSpellType_Worn) > 0 ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextColored(ImColor(0, 210, 210), "%s", myui::IsClicky(ref) ? "Yes" : "No");

			ImGui::TableNextColumn();
			if (!myui::IsClicky(ref) || ref.charges() == 0)
			{
				ImGui::TextUnformatted("None");
			}
			else if (ref.charges() == -1)
			{
				ImGui::TextUnformatted("Infinite");
			}
			else
			{
				ImGui::Text("%d", ref.charges());
			}

			ImGui::TableNextColumn();
			if (def->AugType > 0)
			{
				ImGui::TextColored(ImColor(0, 255, 0), "Yes");
			}
			else
			{
				ImGui::TextUnformatted("No");
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

bool BigBagModule::PassesNameFilter(const myui::ItemRef& ref) const
{
	if (m_filter[0] == 0)
	{
		return true;
	}
	return ci_find_substr(ref.name(), m_filter) != -1;
}

bool BigBagModule::PassesPreset(const myui::ItemRef& ref) const
{
	if (m_preset == 0)
	{
		return true;
	}
	if (myui::IsAugment(ref))
	{
		return false;
	}
	switch (m_preset)
	{
	case 1: return myui::ItemFitsSlot(ref, 13) || myui::ItemFitsSlot(ref, 14);
	case 2: return myui::ItemFitsSlot(ref, 11);
	case 3: return myui::ItemFitsSlot(ref, 0);
	case 4: return myui::ItemFitsSlot(ref, 1) || myui::ItemFitsSlot(ref, 4);
	case 5: return myui::ItemFitsSlot(ref, 3);
	case 6: return myui::ItemFitsSlot(ref, 5);
	case 7: return myui::ItemFitsSlot(ref, 15) || myui::ItemFitsSlot(ref, 16);
	default: return true;
	}
}

void BigBagModule::SortItems(std::vector<myui::ItemRef>& items) const
{
	bool byType = m_ctx.UI->Flag(GetName(), "SortType", true);
	bool byName = m_ctx.UI->Flag(GetName(), "SortName", true);
	bool byStack = m_ctx.UI->Flag(GetName(), "SortStack", true);

	std::sort(items.begin(), items.end(), [&](const myui::ItemRef& a, const myui::ItemRef& b) {
		ItemDefinition* da = a.item->GetItemDefinition();
		ItemDefinition* db = b.item->GetItemDefinition();
		if (!da || !db)
		{
			return false;
		}
		if (byType)
		{
			const char* ta = myui::ItemTypeName(da);
			const char* tb = myui::ItemTypeName(db);
			int c = ci_string_compare(ta ? ta : "", tb ? tb : "");
			if (c != 0)
			{
				return c < 0;
			}
		}
		if (byName)
		{
			int c = ci_string_compare(da->Name, db->Name);
			if (c != 0)
			{
				return c < 0;
			}
		}
		if (byStack)
		{
			return a.stack() > b.stack();
		}
		return false;
	});
}
