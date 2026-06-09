#include "MyInventoryModule.h"

#include "../Core/UiConfig.h"
#include "../Core/InventoryData.h"
#include "../Core/HotkeyCombo.h"
#include "../Core/Actions.h"
#include "../Core/IconHelper.h"
#include "../Core/CharData.h"
#include "../Core/BarEngine.h"
#include "../Core/UiHelpers.h"
#include "../Core/Widgets.h"

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

const ImVec4 kStLabel(0.45f, 0.70f, 0.85f, 1.0f);
const ImVec4 kStValue(0.40f, 0.85f, 0.45f, 1.0f);
const ImVec4 kStWhite(1.0f, 1.0f, 1.0f, 1.0f);
const ImVec4 kStOrange(0.95f, 0.55f, 0.16f, 1.0f);
const float kStValX = 80.0f;

void StatGameTip(const char* tipControl)
{
	if (!tipControl || !ImGui::IsItemHovered())
	{
		return;
	}
	std::string tip = InvControlTooltip(tipControl);
	if (!tip.empty())
	{
		ImGui::SetTooltip("%s", tip.c_str());
	}
}

void StatRow(const char* label, const std::string& value, const char* tipControl)
{
	ImGui::BeginGroup();
	ImGui::TextColored(kStLabel, "%s", label);
	ImGui::SameLine(kStValX);
	ImGui::TextColored(kStValue, "%s", value.c_str());
	ImGui::EndGroup();
	StatGameTip(tipControl);
}
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

		if (ImGui::BeginTabBar("##MyInvTabs"))
		{
			if (ImGui::BeginTabItem("Inventory"))
			{
				if (ImGui::BeginTable("##InvLayout", 3, ImGuiTableFlags_SizingFixedFit))
				{
					float pad = ImGui::GetStyle().CellPadding.x;
					ImGui::TableSetupColumn("##stats", ImGuiTableColumnFlags_WidthFixed, 185.0f);
					ImGui::TableSetupColumn("##pd", ImGuiTableColumnFlags_WidthFixed, cell * 5.0f + pad * 12.0f);
					ImGui::TableSetupColumn("##bags", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableNextRow();
					if (ImGui::TableNextColumn())
					{
						DrawInventoryStats();
					}
					if (ImGui::TableNextColumn())
					{
						DrawPaperdoll(cell, showBackground, highlight);
					}
					if (ImGui::TableNextColumn())
					{
						DrawBags(cell, showBackground, highlight);
						ImGui::Spacing();
						myui::DrawCurrencyRow(m_ctx.Icons, 20.0f * w.iconScale, m_coin, true);
					}
					ImGui::EndTable();
				}

				ImGui::Separator();
				DrawInventoryResists();
				ImGui::Separator();
				myui::DrawDropZones();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Stats"))
			{
				DrawStatsTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::Separator();
		DrawInventoryFooter(w.visible);

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

void MyInventoryModule::DrawStatsTab()
{
	if (!m_ctx.Char)
	{
		return;
	}
	m_ctx.Char->RefreshStats();
	const StatsSnapshot& s = m_ctx.Char->Stats();
	if (!s.valid)
	{
		ImGui::TextDisabled("Stats unavailable");
		return;
	}

	const ImVec4 colLabel(0.45f, 0.70f, 0.85f, 1.0f);
	const ImVec4 colValue(0.40f, 0.85f, 0.45f, 1.0f);
	const ImVec4 colWhite(1.0f, 1.0f, 1.0f, 1.0f);
	const ImVec4 colOrange(0.95f, 0.55f, 0.16f, 1.0f);
	const ImVec4 colHeroic(0.80f, 0.80f, 0.45f, 1.0f);
	const float valX = 160.0f;

	auto plain = [&](const char* label, const std::string& value) {
		ImGui::TextColored(colLabel, "%s", label);
		ImGui::SameLine(valX);
		ImGui::TextColored(colValue, "%s", value.c_str());
	};
	auto cap = [&](const char* label, const StatVal& sv) {
		ImGui::TextColored(colLabel, "%s", label);
		ImGui::SameLine(valX);
		ImGui::TextColored(colValue, "%d", sv.value);
		if (sv.cap >= 0)
		{
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(colWhite, " / %d", sv.cap);
		}
	};
	auto attr = [&](const char* label, const StatVal& sv) {
		cap(label, sv);
		ImGui::SameLine();
		ImGui::TextColored(colHeroic, "+%d", sv.heroic);
	};
	auto curMax = [&](const char* label, int cur, int maxv) {
		ImGui::TextColored(colLabel, "%s", label);
		ImGui::SameLine(valX);
		ImGui::TextColored(cur >= maxv ? colWhite : colOrange, "%d", cur);
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(colWhite, " / ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(colValue, "%d", maxv);
	};

	if (ImGui::BeginTable("##StatsCols", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 260.0f);
		ImGui::TableSetupColumn("##r", ImGuiTableColumnFlags_WidthFixed, 260.0f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		curMax("HP", s.hpCur, s.hpMax);
		curMax("Mana", s.manaCur, s.manaMax);
		curMax("Endurance", s.endCur, s.endMax);
		plain("Armor Class", std::to_string(s.ac));
		plain("Attack", std::to_string(s.attack));
		plain("Haste", fmt::format("{}%", s.hastePct));
		ImGui::SeparatorText("Stats");
		attr("Strength", s.str);
		attr("Stamina", s.sta);
		attr("Agility", s.agi);
		attr("Dexterity", s.dex);
		attr("Wisdom", s.wis);
		attr("Intelligence", s.intel);
		attr("Charisma", s.cha);
		ImGui::SeparatorText("Resists");
		cap("Magic", s.rMagic);
		cap("Fire", s.rFire);
		cap("Cold", s.rCold);
		cap("Disease", s.rDisease);
		cap("Poison", s.rPoison);
		cap("Corruption", s.rCorrupt);
		ImGui::SeparatorText("Mods");
		cap("Combat Effects", s.combatEffects);

		ImGui::TableSetColumnIndex(1);
		plain("Combat HP Regen", std::to_string(s.hpRegen));
		plain("Combat Mana Regen", std::to_string(s.manaRegen));
		plain("Combat End Regen", std::to_string(s.endRegen));
		ImGui::Spacing();
		cap("Spell Shield", s.spellShield);
		cap("Shielding", s.shielding);
		cap("Damage Shielding", s.dmgShield);
		cap("DoT Shielding", s.dotShield);
		cap("DS Mitigation", s.dsMitigation);
		cap("Avoidance", s.avoidance);
		cap("Accuracy", s.accuracy);
		cap("Stun Resist", s.stunResist);
		cap("Strike Through", s.strikeThrough);
		plain("Spell Damage", std::to_string(s.spellDamage));
		ImGui::SeparatorText("Skill Damage Mod");
		static const char* kSkillNames[9] = {
			"Bash", "Backstab", "Dragon Punch", "Eagle Strike", "Flying Kick",
			"Kick", "Round Kick", "Tiger Claw", "Frenzy",
		};
		for (int i = 0; i < 9; ++i)
		{
			cap(kSkillNames[i], s.skillMod[i]);
		}

		ImGui::EndTable();
	}
}

void MyInventoryModule::DrawInventoryStats()
{
	if (!m_ctx.Char)
	{
		return;
	}
	m_ctx.Char->RefreshStats();
	const CharSnapshot& self = m_ctx.Char->Get();
	const StatsSnapshot& st = m_ctx.Char->Stats();

	const ImVec4 colName(0.45f, 0.85f, 0.45f, 1.0f);
	const ImVec4 colGrey(0.60f, 0.60f, 0.60f, 1.0f);

	auto curMaxRow = [&](const char* label, int cur, int maxv, const char* tipControl) {
		ImGui::BeginGroup();
		ImGui::TextColored(kStLabel, "%s", label);
		ImGui::SameLine(kStValX);
		ImGui::TextColored(cur >= maxv ? kStWhite : kStOrange, "%d", cur);
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(kStWhite, " / ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(kStValue, "%d", maxv);
		ImGui::EndGroup();
		StatGameTip(tipControl);
	};

	ImGui::TextColored(colName, "%s", self.name.c_str());
	ImGui::TextColored(kStWhite, "%d %s", self.level,
		st.className.empty() ? self.classShort.c_str() : st.className.c_str());
	if (!st.deityName.empty())
	{
		ImGui::TextColored(colGrey, "%s", st.deityName.c_str());
	}
	ImGui::Separator();

	curMaxRow("HP", self.curHP, self.maxHP, "IWS_HP");
	curMaxRow("MP", self.curMana, self.maxMana, "IWS_Mana");
	curMaxRow("EN", self.curEnd, self.maxEnd, "IWS_Endurance");
	StatRow("AC", std::to_string(st.ac), "IWS_ArmorClass");
	StatRow("ATK", std::to_string(st.attack), "IWS_Attack");
	ImGui::Separator();

	auto barLabelRow = [&](const char* tableId, const char* label, float pct) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%.2f%%", pct);
		if (ImGui::BeginTable(tableId, 2, ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableNextColumn();
			ImGui::TextColored(kStLabel, "%s", label);
			ImGui::TableNextColumn();
			float tw = ImGui::CalcTextSize(buf).x;
			float avail = ImGui::GetContentRegionAvail().x;
			if (avail > tw)
			{
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - tw);
			}
			ImGui::TextColored(kStValue, "%s", buf);
			ImGui::EndTable();
		}
	};

	barLabelRow("##xplbl", "Next Level", self.pctExp);
	myui::DrawStyledBar("##xpbar", self.pctExp, m_ctx.UI->Bar(GetName(), "XP"));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Exp: %.2f%%", self.pctExp);
	}
	barLabelRow("##aaxplbl", "Alt Adv", self.pctAAExp);
	myui::DrawStyledBar("##aaxpbar", self.pctAAExp, m_ctx.UI->Bar(GetName(), "AAExp"));
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("AA Exp: %.2f%%", self.pctAAExp);
	}
	ImGui::Separator();

	auto attrCell = [&](const char* label, int value, const char* tip) {
		ImGui::BeginGroup();
		ImGui::TextColored(kStLabel, "%s", label);
		ImGui::SameLine(42.0f);
		ImGui::TextColored(kStValue, "%d", value);
		ImGui::EndGroup();
		StatGameTip(tip);
	};
	if (ImGui::BeginTable("##attr2", 2, ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("##c0", ImGuiTableColumnFlags_WidthFixed, 88.0f);
		ImGui::TableSetupColumn("##c1", ImGuiTableColumnFlags_WidthFixed, 88.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn(); attrCell("STR", st.str.value, "IWS_Strength");
		ImGui::TableNextColumn(); attrCell("WIS", st.wis.value, "IWS_Wisdom");
		ImGui::TableNextRow();
		ImGui::TableNextColumn(); attrCell("STA", st.sta.value, "IWS_Stamina");
		ImGui::TableNextColumn(); attrCell("INT", st.intel.value, "IWS_Intelligence");
		ImGui::TableNextRow();
		ImGui::TableNextColumn(); attrCell("AGI", st.agi.value, "IWS_Agility");
		ImGui::TableNextColumn(); attrCell("CHA", st.cha.value, "IWS_Charisma");
		ImGui::TableNextRow();
		ImGui::TableNextColumn(); attrCell("DEX", st.dex.value, "IWS_Dexterity");
		ImGui::EndTable();
	}
}

void MyInventoryModule::DrawInventoryResists()
{
	if (!m_ctx.Char)
	{
		return;
	}
	const StatsSnapshot& st = m_ctx.Char->Stats();

	struct ResistEntry { const char* name; int icon; int value; const char* tip; };
	const ResistEntry entries[6] = {
		{ "Poison",     61, st.rPoison.value,  "IWS_Poison" },
		{ "Magic",      69, st.rMagic.value,   "IWS_Magic" },
		{ "Disease",    66, st.rDisease.value, "IWS_Disease" },
		{ "Fire",       52, st.rFire.value,    "IWS_Fire" },
		{ "Cold",       57, st.rCold.value,    "IWS_Cold" },
		{ "Corruption", 65, st.rCorrupt.value, "IWS_Corruption" },
	};

	float h = ImGui::GetTextLineHeight();
	for (int i = 0; i < 6; ++i)
	{
		const ResistEntry& e = entries[i];
		if (i > 0)
		{
			ImGui::SameLine(0.0f, 16.0f);
		}
		ImGui::BeginGroup();
		if (m_ctx.Icons)
		{
			m_ctx.Icons->DrawSpellIcon(e.icon, CXSize(static_cast<int>(h), static_cast<int>(h)),
				MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));
		}
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::TextColored(kStValue, "%d", e.value);
		ImGui::EndGroup();
		if (ImGui::IsItemHovered())
		{
			std::string tip = InvControlTooltip(e.tip);
			ImGui::BeginTooltip();
			ImGui::Text("%s: %d", e.name, e.value);
			if (!tip.empty())
			{
				ImGui::TextColored(kStLabel, "%s", tip.c_str());
			}
			ImGui::EndTooltip();
		}
	}
}

void MyInventoryModule::DrawInventoryFooter(bool& visible)
{
	if (m_ctx.Char)
	{
		const StatsSnapshot& st = m_ctx.Char->Stats();
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(kStLabel, "WEIGHT");
		ImGui::SameLine();
		ImGui::TextColored(st.weight > st.str.value ? kStOrange : kStWhite, "%d", st.weight);
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(kStWhite, " / ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(kStValue, "%d", st.str.value);

		ImGui::SameLine(0.0f, 16.0f);
		int free = myui::GetFreeSlots();
		ImGui::TextColored(free > 3 ? ImVec4(0.47f, 0.86f, 0.47f, 1.0f) : ImVec4(0.90f, 0.63f, 0.24f, 1.0f),
			"Free Slots: %d", free);
		ImGui::SameLine();
	}

	float rightX = ImGui::GetContentRegionMax().x - 55.0f;
	if (ImGui::GetCursorPosX() < rightX)
	{
		ImGui::SetCursorPosX(rightX);
	}
	if (myui::StyledButton("Done"))
	{
		visible = false;
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
		opts.backgroundAnim = myui::WornSlotBackgroundName(slot);
		myui::DrawItemIcon(m_ctx.Icons, ref, opts);

		if (ImGui::IsItemHovered() && !ref.valid())
		{
			ImGui::SetItemTooltip("%s (empty)", myui::WornSlotDisplayName(slot));
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
		ImGui::PushStyleColor(ImGuiCol_Text, myui::kItemNameColor);
		bool selected = ImGui::Selectable(ref.name());
		ImGui::PopStyleColor();
		if (selected)
		{
			std::string preUnequip;
			if (wornSlot == myui::kSlotPrimary && myui::IsTwoHandedWeapon(ref)
				&& myui::WornItem(myui::kSlotSecondary).valid())
			{
				preUnequip = fmt::format("/itemnotify {} leftmouseup",
					myui::WornSlotCommandName(myui::kSlotSecondary));
			}
			myui::SwapToWornSlot(myui::PickupCommand(ref), dropCommand, autoInventory, preUnequip, ref.id());
		}
		if (ImGui::IsItemHovered())
		{
			if (ImGui::GetIO().KeyShift)
			{
				myui::RenderCompareSideBySide(m_ctx.Icons, ref, current);
			}
			else
			{
				myui::RenderCompareTooltip(ref, current);
			}
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
			if (myui::StyledSmallButton(locked ? ICON_FA_LOCK : ICON_FA_UNLOCK))
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
