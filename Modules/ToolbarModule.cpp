#include "ToolbarModule.h"

#include "../Core/UiConfig.h"
#include "../Core/IconHelper.h"
#include "../Core/InventoryData.h"
#include "../Core/CharData.h"
#include "../Core/Widgets.h"

#include "mq/imgui/Widgets.h"

#include <imgui/imgui_internal.h>

#include <algorithm>
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

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x / 3.0f, vp->WorkPos.y), ImGuiCond_FirstUseEver);
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

	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	const float rounding = ImGui::GetStyle().FrameRounding;

	// One interaction item for the whole slot; everything else is drawn manually
	// so the frame/background sit behind the icon and the boundaries are obvious.
	ImGui::InvisibleButton("##slot", ImVec2(size, size));
	const ImGuiID id = ImGui::GetID("##slot");
	const bool hovered = ImGui::IsItemHovered();
	const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

	// Shared styled-widget motion: eased hover, spring press-scale, diagonal sheen.
	const float hi = myui::HoverIntensity(id, hovered);
	const float scale = myui::PressScale(id, ImGui::IsItemActive());

	// Scaled slot rect (drawn only; layout/cursor are unchanged).
	const ImVec2 cen(p0.x + size * 0.5f, p0.y + size * 0.5f);
	const float half = size * 0.5f * scale;
	const ImVec2 q0(cen.x - half, cen.y - half);
	const ImVec2 q1(cen.x + half, cen.y + half);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	if (hi > 0.001f && myui::AnimationsEnabled())
	{
		ImVec4 glow = myui::Soften(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
		glow.w = 1.0f;
		myui::SoftGlowRoundRect(dl, q0, q1, rounding, glow, hi);
	}

	// Slot background, brightening on hover, so each icon reads as its own button.
	const ImVec4 bg = ImLerp(myui::Soften(ImGui::GetStyleColorVec4(ImGuiCol_Button)),
		myui::Soften(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)), ImClamp(hi, 0.0f, 1.0f));
	dl->AddRectFilled(q0, q1, ImGui::GetColorU32(bg), rounding);

	// Icon inset a hair so it never touches the frame, scaled with the slot.
	const float slotPx = size * scale;
	const float inset = std::max(slotPx * 0.08f, 1.0f);
	const int iconPx = std::max(static_cast<int>(slotPx - inset * 2.0f), 1);
	anim->SetCurCell(iconId - 500);
	imgui::DrawTextureAnimation(dl, anim,
		CXPoint(static_cast<int>(q0.x + inset), static_cast<int>(q0.y + inset)),
		CXSize(iconPx, iconPx), MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));

	// Diagonal hover sheen (self-gated by the animation switch), masked to the slot.
	myui::DrawHoverShimmer(dl, q0, q1, rounding, hi);

	// Always-visible theme-accent rim, brightening on hover.
	ImVec4 rim = myui::Soften(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	rim.w = 0.40f + 0.60f * ImClamp(hi, 0.0f, 1.0f);
	dl->AddRect(q0, q1, ImGui::GetColorU32(rim), rounding, 0, std::max(ImGui::GetStyle().FrameBorderSize, 1.0f));

	if (tooltip && hovered)
	{
		ImGui::SetTooltip("%s", tooltip);
	}
	return clicked;
}
