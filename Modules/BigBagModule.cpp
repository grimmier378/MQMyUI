#include "BigBagModule.h"

#include "../Core/UiConfig.h"
#include "../Core/HotkeyCombo.h"
#include "../Core/IconHelper.h"
#include "../Core/CharData.h"
#include "../Core/Actions.h"
#include "../Core/UiHelpers.h"
#include "../Core/Widgets.h"

#include <algorithm>
#include <functional>

namespace
{
const char* kPresetNames[] = { "All", "Armor", "Weapons", "Ranged", "Charm", "Ears", "Face", "Neck", "Rings" };

// Worn slots counted as armor for the Armor category (head, shoulders, arms, back,
// wrists, hands, chest, legs, feet, waist).
const int kArmorSlots[] = {
	myui::kSlotHead, myui::kSlotShoulder, myui::kSlotArms, myui::kSlotBack,
	myui::kSlotWrist1, myui::kSlotWrist2, myui::kSlotHands, myui::kSlotChest,
	myui::kSlotLegs, myui::kSlotFeet, myui::kSlotWaist,
};
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
	float iconSize = 20.0f * w.iconScale;

	ImGui::SetNextWindowSize(ImVec2(520.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI BigBag##MyUIBigBag", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		DrawHeader(iconSize);
		DrawControls();
		ImGui::Separator();

		static const char* const kTabs[] = { "Items", "Clickies", "Augments", "Bank", "Details" };
		m_tab = myui::PillTabBar("##BigBagTabs", kTabs, IM_ARRAYSIZE(kTabs), m_tab);

		// Filter a source list by a per-tab predicate, sort, and render the grid.
		auto collectAndSortDraw = [&](const std::vector<myui::ItemRef>& source,
			const std::function<bool(const myui::ItemRef&)>& predicate) {
			std::vector<myui::ItemRef> items;
			for (const myui::ItemRef& ref : source)
			{
				if (predicate(ref))
				{
					items.push_back(ref);
				}
			}
			SortItems(items);
			DrawItemGrid(items, cell, showBackground);
		};

		if (m_tab == 0)
		{
			ImGui::SetNextItemWidth(140.0f);
			myui::StyledCombo("Category", &m_preset, kPresetNames, IM_ARRAYSIZE(kPresetNames));
			ImGui::SameLine();
			bool useableOnly = ui->Flag(GetName(), "UseableOnly", false);
			if (myui::StyledCheckbox("Useable Only", &useableOnly))
			{
				ui->SetFlag(GetName(), "UseableOnly", useableOnly);
			}

			collectAndSortDraw(myui::GetBagContents(), [&](const myui::ItemRef& ref) {
				return PassesNameFilter(ref) && PassesPreset(ref) && PassesUseableFilter(ref);
			});
		}
		else if (m_tab == 1)
		{
			collectAndSortDraw(myui::GetBagContents(), [&](const myui::ItemRef& ref) {
				return myui::IsClicky(ref) && PassesNameFilter(ref);
			});
		}
		else if (m_tab == 2)
		{
			collectAndSortDraw(myui::GetBagContents(), [&](const myui::ItemRef& ref) {
				return myui::IsAugment(ref) && PassesNameFilter(ref);
			});
		}
		else if (m_tab == 3)
		{
			collectAndSortDraw(myui::GetBank(), [&](const myui::ItemRef& ref) {
				return PassesNameFilter(ref);
			});
		}
		else if (m_tab == 4)
		{
			DrawDetails();
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
	if (myui::StyledButton("Clear"))
	{
		m_filter[0] = 0;
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Sort:");
	ImGui::SameLine();
	bool sortName = ui->Flag(GetName(), "SortName", true);
	if (myui::StyledCheckbox("Name", &sortName))
	{
		ui->SetFlag(GetName(), "SortName", sortName);
	}
	ImGui::SameLine();
	bool sortStack = ui->Flag(GetName(), "SortStack", true);
	if (myui::StyledCheckbox("Stack", &sortStack))
	{
		ui->SetFlag(GetName(), "SortStack", sortStack);
	}
	ImGui::SameLine();
	bool sortType = ui->Flag(GetName(), "SortType", true);
	if (myui::StyledCheckbox("Type", &sortType))
	{
		ui->SetFlag(GetName(), "SortType", sortType);
	}

	myui::DrawDropZones();
}

void BigBagModule::DrawItemGrid(const std::vector<myui::ItemRef>& items, float cellSize, bool showBackground)
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
		opts.annotate = true;
		myui::DrawItemIcon(m_ctx.Icons, items[i], opts);
		ImGui::PopID();
	}
	ImGui::EndChild();
}

void BigBagModule::DrawDetails()
{
	if (myui::StyledButton("Trade Selected Items"))
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
	bool checkAll = myui::StyledButton("Check All");

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
				myui::StyledCheckbox("##t", &checked);
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

bool BigBagModule::PassesUseableFilter(const myui::ItemRef& ref) const
{
	if (!m_ctx.UI->Flag(GetName(), "UseableOnly", false))
	{
		return true;
	}
	return myui::IsUseableGearItem(ref);
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
	case 1:
		for (int slot : kArmorSlots)
		{
			if (myui::ItemFitsSlot(ref, slot))
			{
				return true;
			}
		}
		return false;
	case 2: return myui::ItemFitsSlot(ref, myui::kSlotPrimary) || myui::ItemFitsSlot(ref, myui::kSlotSecondary);
	case 3: return myui::ItemFitsSlot(ref, myui::kSlotRange);
	case 4: return myui::ItemFitsSlot(ref, myui::kSlotCharm);
	case 5: return myui::ItemFitsSlot(ref, myui::kSlotEar1) || myui::ItemFitsSlot(ref, myui::kSlotEar2);
	case 6: return myui::ItemFitsSlot(ref, myui::kSlotFace);
	case 7: return myui::ItemFitsSlot(ref, myui::kSlotNeck);
	case 8: return myui::ItemFitsSlot(ref, myui::kSlotRing1) || myui::ItemFitsSlot(ref, myui::kSlotRing2);
	default: return true;
	}
}

void BigBagModule::SortItems(std::vector<myui::ItemRef>& items) const
{
	bool byType = m_ctx.UI->Flag(GetName(), "SortType", true);
	bool byName = m_ctx.UI->Flag(GetName(), "SortName", true);
	bool byStack = m_ctx.UI->Flag(GetName(), "SortStack", true);

	// Decorate: precompute each item's sort keys once (def-dependent lookups are
	// virtual calls), then sort the decorated entries and write the items back. A
	// null definition is flagged so the comparator preserves the original behavior
	// of returning false whenever either side lacks a definition.
	struct SortKey
	{
		myui::ItemRef ref;
		std::string   typeName;
		std::string   name;
		int           stack = 0;
		bool          hasDef = false;
	};

	std::vector<SortKey> keys;
	keys.reserve(items.size());
	for (const myui::ItemRef& ref : items)
	{
		SortKey key;
		key.ref = ref;
		ItemDefinition* def = ref.item->GetItemDefinition();
		if (def)
		{
			key.hasDef = true;
			const char* type = myui::ItemTypeName(def);
			key.typeName = type ? type : "";
			key.name = def->Name ? def->Name : "";
			key.stack = ref.stack();
		}
		keys.push_back(std::move(key));
	}

	std::sort(keys.begin(), keys.end(), [&](const SortKey& a, const SortKey& b) {
		if (!a.hasDef || !b.hasDef)
		{
			return false;
		}
		if (byType)
		{
			int c = ci_string_compare(a.typeName, b.typeName);
			if (c != 0)
			{
				return c < 0;
			}
		}
		if (byName)
		{
			int c = ci_string_compare(a.name, b.name);
			if (c != 0)
			{
				return c < 0;
			}
		}
		if (byStack)
		{
			return a.stack > b.stack;
		}
		return false;
	});

	for (size_t i = 0; i < keys.size(); ++i)
	{
		items[i] = keys[i].ref;
	}
}
