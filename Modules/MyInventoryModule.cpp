#include "MyInventoryModule.h"

#include "../Core/UiConfig.h"
#include "../Core/InventoryData.h"
#include "../Core/HotkeyCombo.h"
#include "../Core/Actions.h"
#include "../Core/IconHelper.h"
#include "../Core/CharData.h"
#include "../Core/UiHelpers.h"

#include "mq/imgui/Widgets.h"
#include "imgui/fonts/IconsFontAwesome.h"

#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
const int kPaperdollLayout[][5] = {
	{  1, -1,  2, -1,  4 },
	{  3, -1,  5, -1,  6 },
	{  7, -1, 17, -1,  8 },
	{ -1, 20, -1, 18, -1 },
	{  9, 15, 12, 16, 10 },
	{ 21, -1, 19, -1,  0 },
};

const int kWeaponRow[5] = { 13, 14, -1, 11, 22 };
} // namespace

void MyInventoryModule::OnRenderGUI()
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

	ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	float cell = ui->Num(GetName(), "ItemSize", 40.0f) * w.iconScale;
	bool showBackground = ui->Flag(GetName(), "ShowSlotBackground", true);
	bool highlight = false;

	if (ImGui::Begin("MyUI Inventory##MyUIInventory", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		DrawHeader();
		ImGui::Separator();

		if (ImGui::BeginTable("##InvLayout", 2, ImGuiTableFlags_SizingFixedFit))
		{
			float pad = ImGui::GetStyle().CellPadding.x;
			ImGui::TableSetupColumn("##pd", ImGuiTableColumnFlags_WidthFixed, cell * 5.0f + pad * 12.0f);
			ImGui::TableSetupColumn("##bags", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableNextRow();
			if (ImGui::TableNextColumn())
			{
				DrawPaperdoll(cell, showBackground, highlight);
			}
			if (ImGui::TableNextColumn())
			{
				DrawBags(cell, showBackground, highlight);
			}
			ImGui::EndTable();
		}

		ImGui::Separator();
		myui::DrawCurrencyRow(m_ctx.Icons, 20.0f * w.iconScale, m_coin);

		myui::DrawDropZones();

		if (ImGui::Button("Done"))
		{
			w.visible = false;
		}

		ImGui::PopFont();
	}
	ImGui::End();

	DrawContainerWindows(cell, showBackground, highlight);
	myui::DrawCoinQuantityWindow(m_coin);

	if (!w.visible)
	{
		ui->PersistVisibility(GetName());
	}
}

void MyInventoryModule::DrawHeader()
{
	int free = myui::GetFreeSlots();
	ImColor freeColor = free > 3 ? ImColor(120, 220, 120) : ImColor(230, 160, 60);
	ImGui::TextColored(freeColor, "Free Slots: %d", free);

	myui::ItemRef cursor = myui::CursorItem();
	if (cursor.valid())
	{
		ImGui::SameLine();
		ImGui::TextColored(ImColor(120, 200, 230), "Cursor: %s", cursor.name());
	}
}

void MyInventoryModule::DrawPaperdoll(float cellSize, bool showBackground, bool highlight)
{
	auto drawCell = [&](int slot) {
		if (slot < 0)
		{
			ImGui::Dummy(ImVec2(cellSize, cellSize));
			return;
		}

		ImGui::PushID(slot);
		myui::ItemRef ref = myui::WornItem(slot);

		myui::DrawItemOptions opts;
		opts.size = cellSize;
		opts.showBackground = showBackground;
		opts.highlightUseable = highlight;
		opts.handleUse = false;
		opts.handlePopInfo = true;
		myui::DrawItemIcon(m_ctx.Icons, ref, opts);

		if (ImGui::IsItemHovered() && !ref.valid())
		{
			ImGui::SetItemTooltip("%s", myui::WornSlotDisplayName(slot));
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl)
		{
			ImGui::OpenPopup("##swap");
		}
		if (ImGui::BeginPopup("##swap"))
		{
			DrawSwapMenu(slot);
			ImGui::EndPopup();
		}
		ImGui::PopID();
	};

	if (ImGui::BeginTable("##Paperdoll", 5, ImGuiTableFlags_SizingFixedFit))
	{
		for (const auto& row : kPaperdollLayout)
		{
			ImGui::TableNextRow();
			for (int c = 0; c < 5; ++c)
			{
				ImGui::TableSetColumnIndex(c);
				drawCell(row[c]);
			}
		}
		ImGui::EndTable();
	}

	ImGui::Separator();

	if (ImGui::BeginTable("##PaperdollWeapons", 5, ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableNextRow();
		for (int c = 0; c < 5; ++c)
		{
			ImGui::TableSetColumnIndex(c);
			drawCell(kWeaponRow[c]);
		}
		ImGui::EndTable();
	}
}

void MyInventoryModule::DrawSwapMenu(int wornSlot)
{
	myui::ItemRef current = myui::WornItem(wornSlot);
	if (current.valid())
	{
		ImGui::TextDisabled("%s", current.name());
		if (ImGui::MenuItem("Item Info"))
		{
			myui::PopInfoWindow(current);
		}
		ImGui::Separator();
	}

	std::vector<myui::ItemRef> compatible = myui::GetCompatibleItems(wornSlot);
	if (compatible.empty())
	{
		ImGui::TextDisabled("No compatible items");
		return;
	}

	bool autoInventory = m_ctx.UI->Flag(GetName(), "AutoInventoryOnSwap", true);
	std::string dropCommand = fmt::format("/itemnotify {} leftmouseup", myui::WornSlotCommandName(wornSlot));

	std::unordered_set<std::string> seen;
	for (myui::ItemRef& ref : compatible)
	{
		if (!seen.insert(myui::ItemCompareKey(ref)).second)
		{
			continue;
		}

		char idBuf[96];
		sprintf_s(idBuf, "%d_%d_%d_%s", ref.location, ref.slotId, ref.bagSlot, ref.name());
		ImGui::PushID(idBuf);
		if (ImGui::Selectable(ref.name()))
		{
			myui::SwapToWornSlot(myui::PickupCommand(ref), dropCommand, autoInventory);
		}
		if (ImGui::IsItemHovered())
		{
			myui::RenderCompareTooltip(ref, current);
		}
		ImGui::PopID();
	}
}

void MyInventoryModule::DrawBags(float cellSize, bool showBackground, bool highlight)
{
	int count = myui::BagSlotCount();
	int perRow = 2;

	for (int p = 0; p < count; ++p)
	{
		if (p % perRow != 0)
		{
			ImGui::SameLine();
		}

		ImGui::PushID(1000 + p);
		myui::ItemRef bag = myui::BagItem(p);

		myui::DrawItemOptions opts;
		opts.size = cellSize;
		opts.showBackground = showBackground;
		opts.highlightUseable = false;
		opts.handleUse = false;
		opts.handlePopInfo = true;
		myui::DrawItemIcon(m_ctx.Icons, bag, opts);

		if (bag.valid() && ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl)
		{
			if (bag.isContainer())
			{
				if (m_openContainers.count(p))
				{
					m_openContainers.erase(p);
				}
				else
				{
					m_openContainers.insert(p);
				}
			}
			else
			{
				myui::UseItem(bag);
			}
		}
		ImGui::PopID();
	}
}

void MyInventoryModule::DrawContainerWindows(float cellSize, bool showBackground, bool highlight)
{
	UiConfig* ui = m_ctx.UI;
	std::vector<int> open(m_openContainers.begin(), m_openContainers.end());
	for (int p : open)
	{
		myui::ItemRef bag = myui::BagItem(p);
		if (!bag.valid() || !bag.isContainer())
		{
			m_openContainers.erase(p);
			continue;
		}

		std::string lockKey = "BagLock" + std::to_string(p);
		bool locked = ui->Flag(GetName(), lockKey, false);

		bool windowOpen = true;
		char title[128];
		sprintf_s(title, "%s##MyInvContainer_%d", bag.name(), p);

		ImGuiWindowFlags cflags = 0;
		if (locked)
		{
			cflags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		}

		ImGui::SetNextWindowSize(ImVec2(cellSize * 4.0f + 30.0f, 0.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(title, &windowOpen, cflags))
		{
			if (ImGui::SmallButton(locked ? ICON_FA_LOCK : ICON_FA_UNLOCK))
			{
				ui->SetFlag(GetName(), lockKey, !locked);
				ui->PersistField(GetName(), lockKey);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(locked ? "Locked - click to unlock" : "Unlocked - click to lock");
			}
			ImGui::Separator();

			std::vector<myui::ItemRef> contents = myui::BagContents(p);
			float avail = ImGui::GetContentRegionAvail().x;
			int perRow = myui::ColumnsForWidth(avail, cellSize);

			for (size_t i = 0; i < contents.size(); ++i)
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
				myui::DrawItemIcon(m_ctx.Icons, contents[i], opts);
				ImGui::PopID();
			}

			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				windowOpen = false;
			}
		}
		ImGui::End();

		if (!windowOpen)
		{
			m_openContainers.erase(p);
		}
	}
}
