#include "ToolbarModule.h"

#include "../Core/UiConfig.h"
#include "../Core/IconHelper.h"
#include "../Core/InventoryData.h"
#include "../Core/CharData.h"

#include "mq/imgui/Widgets.h"

#include <cstdio>

void ToolbarModule::OnRenderGUI()
{
	UiConfig* ui = m_ctx.UI;
	if (!ui)
	{
		return;
	}

	WindowConfig& w = ui->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame())
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	float icon = ui->Num(GetName(), "ItemSize", 32.0f) * w.iconScale;
	ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
	float pad = ImGui::GetStyle().WindowPadding.x * 2.0f;

	ImGui::SetNextWindowSize(ImVec2((icon + spacing.x) * 5.0f + pad, icon + ImGui::GetStyle().WindowPadding.y * 2.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Toolbar##MyUIToolbar", nullptr, flags))
	{
		float avail = ImGui::GetContentRegionAvail().x;
		int cols = static_cast<int>(avail / (icon + spacing.x));
		if (cols < 1)
		{
			cols = 1;
		}

		int slot = 0;
		auto place = [&]() {
			if (slot % cols != 0)
			{
				ImGui::SameLine();
			}
			++slot;
		};

		place();
		ImGui::PushID("Settings");
		if (IconButton(6850, icon, "Settings"))
		{
			ui->ToggleVisible("Settings");
		}
		if (ImGui::BeginPopupContextItem("##ToolbarSettingsMenu"))
		{
			if (ImGui::MenuItem(w.locked ? "Unlock Toolbar" : "Lock Toolbar"))
			{
				w.locked = !w.locked;
				ui->PersistField("Toolbar", "Locked");
			}
			if (ImGui::MenuItem("Hide Toolbar"))
			{
				w.visible = false;
				ui->PersistVisibility("Toolbar");
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		place();
		ImGui::PushID("BigBag");
		int free = myui::GetFreeSlots();
		int warn = static_cast<int>(ui->Num("BigBag", "MinSlotsWarn", 3.0f));
		bool low = free <= warn;
		ImVec2 origin = ImGui::GetCursorScreenPos();
		if (IconButton(low ? 3632 : 3635, icon, "Big Bag"))
		{
			ui->ToggleVisible("BigBag");
		}
		{
			char buf[16];
			sprintf_s(buf, "%d", free);
			ImVec2 ts = ImGui::CalcTextSize(buf);
			ImU32 col = low ? IM_COL32(0, 220, 220, 255) : IM_COL32(235, 235, 235, 255);
			ImGui::GetWindowDrawList()->AddText(ImVec2(origin.x + icon - ts.x - 2.0f, origin.y + icon - ts.y - 1.0f), col, buf);
		}
		ImGui::PopID();

		place();
		ImGui::PushID("Inventory");
		if (IconButton(3515, icon, "Inventory"))
		{
			ui->ToggleVisible("Inventory");
		}
		ImGui::PopID();

		place();
		ImGui::PushID("iTrack");
		if (IconButton(1147, icon, "Item Tracker"))
		{
			ui->ToggleVisible("iTrack");
		}
		ImGui::PopID();

		place();
		ImGui::PushID("AA");
		if (IconButton(2305, icon, "AA"))
		{
			ui->ToggleVisible("AA");
		}
		ImGui::PopID();
	}
	ImGui::End();
}

bool ToolbarModule::IconButton(int iconId, float size, const char* tooltip)
{
	CTextureAnimation* anim = m_ctx.Icons ? m_ctx.Icons->ItemIcon() : nullptr;
	if (!anim)
	{
		ImGui::Dummy(ImVec2(size, size));
		return false;
	}

	anim->SetCurCell(iconId - 500);
	imgui::DrawTextureAnimation(anim, CXSize(static_cast<int>(size), static_cast<int>(size)),
		MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));

	if (tooltip && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", tooltip);
	}
	return ImGui::IsItemClicked(ImGuiMouseButton_Left);
}
