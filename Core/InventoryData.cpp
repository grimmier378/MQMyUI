#include "InventoryData.h"
#include "IconHelper.h"
#include "Actions.h"
#include "Widgets.h"

#include "mq/imgui/Widgets.h"
#include "imgui/fonts/IconsFontAwesome.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace myui
{
namespace
{
const char* kWornCommandNames[] = {
	"charm", "leftear", "head", "face", "rightear", "neck", "shoulder", "arms", "back",
	"leftwrist", "rightwrist", "ranged", "hands", "mainhand", "offhand", "leftfinger",
	"rightfinger", "chest", "legs", "feet", "waist", "powersource", "ammo",
};

const char* kWornDisplayNames[] = {
	"Charm", "Left Ear", "Head", "Face", "Right Ear", "Neck", "Shoulder", "Arms", "Back",
	"Left Wrist", "Right Wrist", "Range", "Hands", "Primary", "Secondary", "Left Finger",
	"Right Finger", "Chest", "Legs", "Feet", "Waist", "Power Source", "Ammo",
};

const char* kWornBackgroundNames[] = {
	"A_InvCharm", "A_InvEar", "A_InvHead", "A_InvFace", "A_InvEar", "A_InvNeck", "A_InvShoulders",
	"A_InvArms", "A_InvBack", "A_InvWrist", "A_InvWrist", "A_InvRange", "A_InvHands", "A_InvPrimary",
	"A_InvSecondary", "A_InvRing", "A_InvRing", "A_InvChest", "A_InvLegs", "A_InvFeet", "A_InvWaist",
	"A_InvPowerSource", "A_InvAmmo",
};

constexpr int kWornCount = 23;

int PlayerLevel()
{
	return pLocalPC ? pLocalPC->GetLevel() : 0;
}

struct PoppedItem
{
	ItemRef ref;
	ImVec2  spawnPos;
};

std::vector<PoppedItem>& PoppedItems()
{
	static std::vector<PoppedItem> items;
	return items;
}

struct AugInsertState
{
	int         phase = 0;
	ItemRef     target;
	int         slot = -1;
	int         waitTicks = 0;
	std::string augName;
	std::string targetName;
	ImVec2      popPos = ImVec2(0.0f, 0.0f);
};

AugInsertState& AugInsert()
{
	static AugInsertState state;
	return state;
}

const ImColor kColGreen(40, 200, 40);
const ImColor kColOrange(242, 140, 40);
const ImColor kColGrey(150, 150, 150);
const ImColor kColYellow(255, 255, 0);
const ImColor kColHeroicSoft(200, 200, 110);
const ImColor kColTeal(0, 210, 210);
const ImColor kColRed(220, 90, 90);
const ImColor kColPink(230, 130, 160);
const ImColor kColSoftBlue(120, 170, 230);

const char* SizeName(int size)
{
	static const char* kSizes[] = { "Tiny", "Small", "Medium", "Large", "Giant" };
	return (size >= 0 && size <= 4) ? kSizes[size] : "Unknown";
}

std::string FormatThousands(int value)
{
	bool negative = value < 0;
	std::string digits = std::to_string(negative ? -static_cast<long long>(value) : value);
	std::string out;
	int count = 0;
	for (auto it = digits.rbegin(); it != digits.rend(); ++it)
	{
		if (count != 0 && count % 3 == 0)
		{
			out.push_back(',');
		}
		out.push_back(*it);
		++count;
	}
	if (negative)
	{
		out.push_back('-');
	}
	std::reverse(out.begin(), out.end());
	return out;
}

constexpr int kAllThreshold = 16;

std::string ClassList(ItemDefinition* def)
{
	int count = 0;
	for (int i = 0; i < TotalPlayerClasses; ++i)
	{
		if (def->Classes & (1 << i))
		{
			++count;
		}
	}
	if (count == 0)
	{
		return "";
	}
	if (count >= kAllThreshold)
	{
		return "All";
	}

	std::string out;
	for (int i = 0; i < TotalPlayerClasses; ++i)
	{
		if (def->Classes & (1 << i))
		{
			if (!out.empty())
			{
				out += " ";
			}
			out += pEverQuest->GetClassThreeLetterCode(i + 1);
		}
	}
	return out;
}

const char* RaceShortCode(const char* fullName)
{
	static const std::unordered_map<std::string, const char*> kRaceShort = {
		{ "Human", "HUM" }, { "Barbarian", "BAR" }, { "Erudite", "ERU" }, { "Wood Elf", "ELF" },
		{ "High Elf", "HIE" }, { "Dark Elf", "DEF" }, { "Half Elf", "HEF" }, { "Dwarf", "DWF" },
		{ "Troll", "TRL" }, { "Ogre", "OGR" }, { "Halfling", "HFL" }, { "Gnome", "GNM" },
		{ "Iksar", "IKS" }, { "Vah Shir", "VAH" }, { "Froglok", "FRG" }, { "Drakkin", "DRK" },
	};
	auto it = kRaceShort.find(fullName ? fullName : "");
	return it != kRaceShort.end() ? it->second : nullptr;
}

std::string RaceList(ItemDefinition* def)
{
	int count = 0;
	for (int i = 0; i < NUM_RACES; ++i)
	{
		if (def->Races & (1 << i))
		{
			++count;
		}
	}
	if (count == 0)
	{
		return "";
	}
	if (count >= kAllThreshold)
	{
		return "All";
	}

	std::string out;
	for (int i = 0; i < NUM_RACES; ++i)
	{
		if (!(def->Races & (1 << i)))
		{
			continue;
		}
		int raceId = i + 1;
		switch (i)
		{
		case 12: raceId = 128; break;
		case 13: raceId = 130; break;
		case 14: raceId = 330; break;
		case 15: raceId = 522; break;
		default: break;
		}
		const char* shortCode = RaceShortCode(pEverQuest ? pEverQuest->GetRaceDesc(raceId) : nullptr);
		if (shortCode)
		{
			if (!out.empty())
			{
				out += " ";
			}
			out += shortCode;
		}
	}
	return out;
}

void EffectBlock(const char* label, ItemDefinition* def, ItemSpellTypes type, bool interactive)
{
	int spellId = def->SpellData.GetSpellId(type);
	if (spellId <= 0)
	{
		return;
	}
	EQ_Spell* spell = GetSpellByID(spellId);
	if (!spell)
	{
		return;
	}

	ImGui::Dummy(ImVec2(10.0f, 10.0f));
	ImGui::Text("%s:", label);
	ImGui::SameLine();
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 290.0f);
	ImGui::TextColored(kColTeal, "%s", spell->Name);
	if (interactive && ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		ImGui::SetItemTooltip("Click to inspect spell");
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && pSpellDisplayManager)
		{
			pSpellDisplayManager->ShowSpell(spellId, true, true, SpellDisplayType_SpellBookWnd);
		}
	}

	char szSlotInfo[2048] = { 0 };
	ShowSpellSlotInfo(spell, szSlotInfo, sizeof(szSlotInfo), "\n");
	if (szSlotInfo[0])
	{
		ImGui::Indent(5.0f);
		ImGui::TextColored(kColSoftBlue, "%s", szSlotInfo);
		ImGui::Unindent(5.0f);
	}
	ImGui::PopTextWrapPos();
}

void PairInt(const char* label, int value, const ImColor& color)
{
	if (value == 0)
	{
		return;
	}
	ImGui::TableNextColumn();
	ImGui::Text("%s:", label);
	ImGui::SameLine();
	ImGui::TextColored(color, "%d", value);
}

void PairStat(const char* label, int base, int heroic, const ImColor& baseColor)
{
	if (base == 0 && heroic == 0)
	{
		return;
	}
	ImGui::TableNextColumn();
	ImGui::Text("%s:", label);
	ImGui::SameLine();
	ImGui::TextColored(baseColor, "%d", base);
	if (heroic != 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(kColYellow, "+%d", heroic);
	}
}

std::string WornSlotList(ItemDefinition* def)
{
	std::string out;
	for (int i = 0; i < kWornCount; ++i)
	{
		if (def->EquipSlots & (1 << i))
		{
			if (!out.empty())
			{
				out += " ";
			}
			out += kWornDisplayNames[i];
		}
	}
	return out;
}

void LabelValueRow(const char* label, const std::string& value)
{
	if (value.empty())
	{
		return;
	}
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("%s", label);
	ImGui::TableNextColumn();
	ImGui::PushTextWrapPos(ImGui::GetColumnWidth());
	ImGui::TextColored(kColGrey, "%s", value.c_str());
	ImGui::PopTextWrapPos();
}

template <typename T>
int AugmentedStat(const ItemRef& ref, T ItemDefinition::* field)
{
	if (!ref.valid())
	{
		return 0;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	if (!def)
	{
		return 0;
	}
	int total = static_cast<int>(def->*field);
	for (int i = 0; i < MAX_AUG_SOCKETS; ++i)
	{
		if (!def->AugData.IsSocketValid(i))
		{
			continue;
		}
		ItemPtr aug = ref.item->GetHeldItem(i);
		if (aug)
		{
			if (ItemDefinition* augDef = aug->GetItemDefinition())
			{
				total += static_cast<int>(augDef->*field);
			}
		}
	}
	return total;
}

int AugmentedStatFn(const ItemRef& ref, int (ItemBase::*getter)() const)
{
	if (!ref.valid())
	{
		return 0;
	}
	int total = ((*ref.item).*getter)();
	ItemDefinition* def = ref.item->GetItemDefinition();
	if (!def)
	{
		return total;
	}
	for (int i = 0; i < MAX_AUG_SOCKETS; ++i)
	{
		if (!def->AugData.IsSocketValid(i))
		{
			continue;
		}
		ItemPtr aug = ref.item->GetHeldItem(i);
		if (aug)
		{
			total += ((*aug).*getter)();
		}
	}
	return total;
}

void DrawAugStat(const char* label, int value, const ImColor& color)
{
	if (value == 0)
	{
		return;
	}
	ImGui::Text("%s:", label);
	ImGui::SameLine();
	ImGui::TextColored(color, "%+d", value);
}

void DrawAugAdditions(const ItemPtr& aug)
{
	if (!aug)
	{
		return;
	}
	ItemDefinition* a = aug->GetItemDefinition();
	if (!a)
	{
		return;
	}
	ImGui::TextColored(kColTeal, "%s", a->Name);
	ImGui::Separator();
	DrawAugStat("AC", a->AC, kColTeal);
	DrawAugStat("HP", a->HP, kColPink);
	DrawAugStat("Mana", a->Mana, kColTeal);
	DrawAugStat("End", a->Endurance, kColYellow);
	DrawAugStat("STR", a->STR, kColOrange);
	DrawAugStat("STA", a->STA, kColOrange);
	DrawAugStat("AGI", a->AGI, kColOrange);
	DrawAugStat("DEX", a->DEX, kColOrange);
	DrawAugStat("WIS", a->WIS, kColOrange);
	DrawAugStat("INT", a->INT, kColOrange);
	DrawAugStat("CHA", a->CHA, kColOrange);
	DrawAugStat("Magic", a->SvMagic, kColGreen);
	DrawAugStat("Fire", a->SvFire, kColGreen);
	DrawAugStat("Cold", a->SvCold, kColGreen);
	DrawAugStat("Disease", a->SvDisease, kColGreen);
	DrawAugStat("Poison", a->SvPoison, kColGreen);
	DrawAugStat("Corruption", a->SvCorruption, kColGreen);
	DrawAugStat("HP Regen", a->HPRegen, kColPink);
	DrawAugStat("Mana Regen", a->ManaRegen, kColTeal);
	DrawAugStat("End Regen", a->EnduranceRegen, kColYellow);
	DrawAugStat("Haste", a->Haste, kColGreen);
}

void InsertAugment(const ItemPtr& target, int slot)
{
	if (!pItemDisplayManager || !target)
	{
		return;
	}
	int idx = pItemDisplayManager->FindWindow(true);
	if (idx == -1)
	{
		idx = pItemDisplayManager->CreateWindowInstance();
	}
	CItemDisplayWnd* wnd = pItemDisplayManager->GetWindow(idx);
	if (!wnd)
	{
		return;
	}
	wnd->SetItem(target, 0);
	wnd->InsertAugmentRequest(slot);
}

CXWnd* AugConfirmDialog()
{
	CXWnd* dlg = FindMQ2Window("ConfirmationDialogBox");
	return (dlg && dlg->IsVisible()) ? dlg : nullptr;
}

void RequestAugInsert(const ItemRef& target, int slot, const char* augName)
{
	AugInsertState& s = AugInsert();
	s.phase = 1;
	s.target = target;
	s.slot = slot;
	s.waitTicks = 0;
	s.augName = augName ? augName : "";
	s.targetName = target.name();
	s.popPos = ImGui::GetMousePos();
}

void DrawAugConfirmPopup()
{
	AugInsertState& s = AugInsert();
	if (s.phase != 1)
	{
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(s.popPos.x + 12.0f, s.popPos.y + 12.0f), ImGuiCond_Appearing);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
	bool open = true;
	if (ImGui::Begin("Insert Augment?##AugInsertConfirm", &open, flags))
	{
		ImGui::Text("Insert augment:");
		ImGui::TextColored(kColTeal, "%s", s.augName.c_str());
		ImGui::Text("into:");
		ImGui::TextColored(kColYellow, "%s", s.targetName.c_str());
		ImGui::Separator();
		if (myui::StyledButton("Yes##auginsert"))
		{
			s.phase = 2;
		}
		ImGui::SameLine();
		if (myui::StyledButton("No##auginsert"))
		{
			s.phase = 0;
		}
	}
	ImGui::End();

	if (!open)
	{
		s.phase = 0;
	}
}

void RenderItemInfo(IconHelper* icons, const ItemRef& ref, bool interactive)
{
	if (!ref.valid())
	{
		return;
	}

	ItemDefinition* def = ref.item->GetItemDefinition();
	if (!def)
	{
		return;
	}

	bool tooHigh = def->RequiredLevel > PlayerLevel();
	bool canUse = pLocalPC && pLocalPC->CanUseItem(ref.item, true, false);

	float headerY = ImGui::GetCursorPosY();

	ImGui::Text("Item:");
	ImGui::SameLine();
	ImColor nameColor = canUse ? kColGreen : (tooHigh ? kColOrange : kColGrey);
	ImGui::TextColored(nameColor, "%s", def->Name);
	if (interactive && ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("Click to copy item Name to clipboard");
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			ImGui::LogToClipboard();
			ImGui::LogText("%s", def->Name);
			ImGui::LogFinish();
		}
	}

	ImGui::Text("Item ID:");
	ImGui::SameLine();
	ImGui::TextColored(kColYellow, "%d", def->ItemNumber);
	if (interactive && ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("Click to copy item ID to clipboard");
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			ImGui::LogToClipboard();
			ImGui::LogText("%d", def->ItemNumber);
			ImGui::LogFinish();
		}
	}

	if (icons && interactive)
	{
		CTextureAnimation* anim = icons->ItemIcon();
		if (anim)
		{
			const float iconSize = 30.0f;
			ImVec2 resume = ImGui::GetCursorPos();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - iconSize - 12.0f);
			ImGui::SetCursorPosY(headerY);
			int cell = ref.icon() > 0 ? ref.icon() - 500 : 0;
			anim->SetCurCell(cell);
			imgui::DrawTextureAnimation(anim, CXSize(static_cast<int>(iconSize), static_cast<int>(iconSize)),
				MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));
			ImGui::SetCursorPos(resume);
		}
	}

	ImGui::Spacing();

	const char* typeName = ItemTypeName(def);
	ImGui::Text("Type:");
	ImGui::SameLine();
	ImGui::TextColored(kColTeal, "%s", typeName ? typeName : "Unknown");

	ImGui::Text("Size:");
	ImGui::SameLine();
	ImGui::TextColored(kColYellow, "%s", SizeName(def->Size));

	std::string restrictions;
	auto addRestriction = [&](const char* text) {
		if (!restrictions.empty())
		{
			restrictions += ", ";
		}
		restrictions += text;
	};
	if (def->Magic)        { addRestriction("Magic"); }
	if (!def->IsDroppable) { addRestriction("No Drop"); }
	if (def->NoRent)       { addRestriction("No Rent"); }
	if (def->Lore != 0)    { addRestriction("Lore"); }
	if (def->Attuneable)   { addRestriction("Attuneable"); }
	if (!restrictions.empty())
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextColored(kColGrey, "%s", restrictions.c_str());
		ImGui::PopTextWrapPos();
	}

	if (ImGui::BeginTable("##basicinfo", 2, ImGuiTableFlags_None))
	{
		ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthFixed, 240.0f);
		LabelValueRow("Classes:", ClassList(def));
		LabelValueRow("Races:", RaceList(def));
		LabelValueRow("Slots:", WornSlotList(def));
		ImGui::EndTable();
	}

	if (ImGui::BeginTable("##lvlinfo", 2, ImGuiTableFlags_None))
	{
		ImGui::TableSetupColumn("##a", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableNextRow();
		if (def->RequiredLevel > 0)
		{
			ImGui::TableNextColumn();
			ImGui::Text("Req Lvl:");
			ImGui::SameLine();
			ImGui::TextColored(tooHigh ? kColOrange : kColGreen, "%d", def->RequiredLevel);
		}
		if (def->RecommendedLevel > 0)
		{
			ImGui::TableNextColumn();
			ImGui::Text("Rec Lvl:");
			ImGui::SameLine();
			ImGui::TextColored(kColSoftBlue, "%d", def->RecommendedLevel);
		}
		ImGui::TableNextColumn();
		ImGui::Text("Weight:");
		ImGui::SameLine();
		ImGui::TextColored(kColPink, "%.1f", def->Weight / 10.0f);
		if (def->Slots > 0)
		{
			ImGui::TableNextColumn();
			ImGui::Text("Slots:");
			ImGui::SameLine();
			ImGui::TextColored(kColYellow, "%d", def->Slots);
			if (def->SizeCapacity > 0)
			{
				ImGui::TableNextColumn();
				ImGui::Text("Bag Size:");
				ImGui::SameLine();
				ImGui::TextColored(kColTeal, "%s", SizeName(def->SizeCapacity));
			}
		}
		if (def->StackSize > 1)
		{
			ImGui::TableNextColumn();
			ImGui::Text("Qty:");
			ImGui::SameLine();
			ImGui::TextColored(kColYellow, "%d", ref.stack());
			ImGui::SameLine();
			ImGui::Text("/");
			ImGui::SameLine();
			ImGui::TextColored(kColTeal, "%d", def->StackSize);
		}
		ImGui::EndTable();
	}

	auto augTotal = [&](auto field) -> int {
		return AugmentedStat(ref, field);
	};
	auto augTotalFn = [&](int (ItemBase::*getter)() const) -> int {
		return AugmentedStatFn(ref, getter);
	};

	if (ImGui::BeginTable("##dmgstats", 2, ImGuiTableFlags_None))
	{
		ImGui::TableSetupColumn("##a", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableNextRow();
		PairInt("Dmg", augTotal(&ItemDefinition::Damage), kColPink);
		PairInt("Dly", def->Delay, kColYellow);
		PairInt("Haste", augTotal(&ItemDefinition::Haste), kColGreen);
		PairInt("Dmg Shield", augTotalFn(&ItemBase::GetDamShield), kColYellow);
		PairInt("DS Mit", augTotalFn(&ItemBase::GetDamageShieldMitigation), kColTeal);
		PairInt("Avoidance", augTotalFn(&ItemBase::GetAvoidance), kColGreen);
		PairInt("DoT Shielding", augTotalFn(&ItemBase::GetDoTShielding), kColYellow);
		PairInt("Accuracy", augTotalFn(&ItemBase::GetAccuracy), kColGreen);
		PairInt("Spell Shield", augTotalFn(&ItemBase::GetSpellShield), kColTeal);
		PairInt("Heal Amt", augTotal(&ItemDefinition::HealAmount), kColPink);
		PairInt("Spell Dmg", augTotal(&ItemDefinition::SpellDamage), kColTeal);
		PairInt("Stun Res", augTotalFn(&ItemBase::GetStunResist), kColGreen);
		PairInt("Clairvoyance", augTotal(&ItemDefinition::Clairvoyance), kColGreen);
		int dmgTotal = augTotal(&ItemDefinition::Damage);
		if (dmgTotal > 0 && def->Delay > 0)
		{
			ImGui::TableNextColumn();
			ImGui::Text("Ratio:");
			ImGui::SameLine();
			ImGui::TextColored(kColTeal, "%.2f", static_cast<float>(dmgTotal) / static_cast<float>(def->Delay));
		}
		ImGui::EndTable();
	}

	bool hasBase = augTotal(&ItemDefinition::AC) || augTotal(&ItemDefinition::HP) || augTotal(&ItemDefinition::Mana)
		|| augTotal(&ItemDefinition::Endurance) || augTotal(&ItemDefinition::HPRegen)
		|| augTotal(&ItemDefinition::ManaRegen) || augTotal(&ItemDefinition::EnduranceRegen);
	if (hasBase)
	{
		ImGui::SeparatorText("Stats");
		if (ImGui::BeginTable("##basestats", 2, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("##a", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableNextRow();
			PairInt("AC", augTotal(&ItemDefinition::AC), kColTeal);
			PairInt("HPs", augTotal(&ItemDefinition::HP), kColPink);
			PairInt("Mana", augTotal(&ItemDefinition::Mana), kColTeal);
			PairInt("End", augTotal(&ItemDefinition::Endurance), kColYellow);
			PairInt("HP Regen", augTotal(&ItemDefinition::HPRegen), kColPink);
			PairInt("Mana Regen", augTotal(&ItemDefinition::ManaRegen), kColTeal);
			PairInt("End Regen", augTotal(&ItemDefinition::EnduranceRegen), kColYellow);
			ImGui::EndTable();
		}
	}

	bool hasStats = augTotal(&ItemDefinition::STR) || augTotal(&ItemDefinition::STA) || augTotal(&ItemDefinition::AGI)
		|| augTotal(&ItemDefinition::DEX) || augTotal(&ItemDefinition::WIS) || augTotal(&ItemDefinition::INT)
		|| augTotal(&ItemDefinition::CHA);
	if (hasStats)
	{
		if (ImGui::BeginTable("##attrstats", 2, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("##a", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableNextRow();
			PairStat("STR", augTotal(&ItemDefinition::STR), augTotal(&ItemDefinition::HeroicSTR), kColOrange);
			PairStat("STA", augTotal(&ItemDefinition::STA), augTotal(&ItemDefinition::HeroicSTA), kColOrange);
			PairStat("AGI", augTotal(&ItemDefinition::AGI), augTotal(&ItemDefinition::HeroicAGI), kColOrange);
			PairStat("DEX", augTotal(&ItemDefinition::DEX), augTotal(&ItemDefinition::HeroicDEX), kColOrange);
			PairStat("WIS", augTotal(&ItemDefinition::WIS), augTotal(&ItemDefinition::HeroicWIS), kColOrange);
			PairStat("INT", augTotal(&ItemDefinition::INT), augTotal(&ItemDefinition::HeroicINT), kColOrange);
			PairStat("CHA", augTotal(&ItemDefinition::CHA), augTotal(&ItemDefinition::HeroicCHA), kColOrange);
			ImGui::EndTable();
		}
	}

	bool hasResists = augTotal(&ItemDefinition::SvMagic) || augTotal(&ItemDefinition::SvFire) || augTotal(&ItemDefinition::SvCold)
		|| augTotal(&ItemDefinition::SvDisease) || augTotal(&ItemDefinition::SvPoison) || augTotal(&ItemDefinition::SvCorruption);
	if (hasResists)
	{
		ImGui::SeparatorText("Resists");
		if (ImGui::BeginTable("##resists", 2, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("##a", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableNextRow();
			PairStat("Magic", augTotal(&ItemDefinition::SvMagic), augTotalFn(&ItemBase::GetHeroicSvMagic), kColGreen);
			PairStat("Fire", augTotal(&ItemDefinition::SvFire), augTotalFn(&ItemBase::GetHeroicSvFire), kColGreen);
			PairStat("Cold", augTotal(&ItemDefinition::SvCold), augTotalFn(&ItemBase::GetHeroicSvCold), kColGreen);
			PairStat("Disease", augTotal(&ItemDefinition::SvDisease), augTotalFn(&ItemBase::GetHeroicSvDisease), kColGreen);
			PairStat("Poison", augTotal(&ItemDefinition::SvPoison), augTotalFn(&ItemBase::GetHeroicSvPoison), kColGreen);
			PairStat("Corruption", augTotal(&ItemDefinition::SvCorruption), augTotalFn(&ItemBase::GetHeroicSvCorruption), kColGreen);
			ImGui::EndTable();
		}
	}

	bool anySocket = false;
	for (int i = 0; i < MAX_AUG_SOCKETS; ++i)
	{
		if (def->AugData.IsSocketValid(i))
		{
			anySocket = true;
			break;
		}
	}
	if (anySocket)
	{
		ImGui::SeparatorText("Augments");
		for (int i = 0; i < MAX_AUG_SOCKETS; ++i)
		{
			if (!def->AugData.IsSocketValid(i))
			{
				continue;
			}
			int socketType = def->AugData.GetSocketType(i);
			ItemPtr aug = ref.item->GetHeldItem(i);
			ImGui::Text("Slot %d:", i + 1);
			ImGui::SameLine();
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 290.0f);
			if (aug)
			{
				ImGui::BeginGroup();
				if (icons && aug->GetIconID() > 0)
				{
					CTextureAnimation* anim = icons->ItemIcon();
					if (anim)
					{
						anim->SetCurCell(aug->GetIconID() - 500);
						imgui::DrawTextureAnimation(anim, CXSize(16, 16), MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));
						ImGui::SameLine(0.0f, 4.0f);
					}
				}
				ImGui::TextColored(kColTeal, "%s Type (%d)", aug->GetName(), socketType);
				ImGui::EndGroup();
				if (interactive && ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					ImGui::BeginTooltip();
					DrawAugAdditions(aug);
					ImGui::Separator();
					ImGui::TextColored(kColGrey, "Click to pop out this augment");
					ImGui::EndTooltip();
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						ItemRef augRef;
						augRef.item = aug;
						augRef.location = ItemLoc_Bag;
						PopInfoWindow(augRef);
					}
				}
			}
			else
			{
				ItemRef cursor = interactive ? CursorItem() : ItemRef{};
				bool fits = cursor.valid() && ref.item->CanGemFitInSlot(cursor.item, i, true, true) == 0;
				ImGui::TextColored(fits ? kColGreen : kColTeal, "Empty Type (%d)", socketType);
				if (interactive && ImGui::IsItemHovered())
				{
					if (!cursor.valid())
					{
						ImGui::SetItemTooltip("Pick up an augment, then click to insert it here");
					}
					else if (fits)
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						ImGui::SetItemTooltip("Click to insert %s", cursor.name());
						if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						{
							RequestAugInsert(ref, i, cursor.name());
						}
					}
					else
					{
						ImGui::SetItemTooltip("%s does not fit this slot", cursor.name());
					}
				}
			}
			ImGui::PopTextWrapPos();
		}
	}

	{
		bool hasClicky = def->SpellData.GetSpellId(ItemSpellType_Clicky) > 0;
		bool hasEffect = hasClicky
			|| def->SpellData.GetSpellId(ItemSpellType_Proc) > 0
			|| def->SpellData.GetSpellId(ItemSpellType_Worn) > 0
			|| def->SpellData.GetSpellId(ItemSpellType_Focus) > 0
			|| def->SpellData.GetSpellId(ItemSpellType_Focus2) > 0;
		if (hasEffect)
		{
			ImGui::SeparatorText("Efx");
			if (hasClicky)
			{
				int charges = ref.charges();
				ImGui::Text("Charges:");
				ImGui::SameLine();
				if (charges < 0)
				{
					ImGui::TextColored(kColYellow, "Infinite");
				}
				else
				{
					ImGui::TextColored(kColYellow, "%d", charges);
				}
			}
			EffectBlock("Clicky", def, ItemSpellType_Clicky, interactive);
			EffectBlock("Proc", def, ItemSpellType_Proc, interactive);
			EffectBlock("Worn", def, ItemSpellType_Worn, interactive);
			EffectBlock("Focus", def, ItemSpellType_Focus, interactive);
			EffectBlock("Focus 2", def, ItemSpellType_Focus2, interactive);
		}
	}

	ImGui::SeparatorText("Value");
	int cost = def->Cost;
	ImGui::TextColored(kColGrey, "Value: %dp %dg %ds %dc",
		cost / 1000, (cost / 100) % 10, (cost / 10) % 10, cost % 10);
	if (def->Favor > 0)
	{
		ImGui::Text("Tribute:");
		ImGui::SameLine();
		ImGui::TextColored(kColYellow, "%d", def->Favor);
	}
}

void CompareLine(const char* label, int candidate, int equipped)
{
	if (candidate == 0 && equipped == 0)
	{
		return;
	}
	int delta = candidate - equipped;
	ImColor color = delta > 0 ? kColGreen : (delta < 0 ? kColRed : kColGrey);
	ImGui::Text("%s:", label);
	ImGui::SameLine();
	if (delta != 0)
	{
		ImGui::TextColored(color, "%d (%+d)", candidate, delta);
	}
	else
	{
		ImGui::TextColored(kColGrey, "%d", candidate);
	}
}

void CompareValue(int candidate, int equipped)
{
	int delta = candidate - equipped;
	if (delta != 0)
	{
		ImColor color = delta > 0 ? kColGreen : kColRed;
		ImGui::TextColored(color, "%d (%+d)", candidate, delta);
	}
	else
	{
		ImGui::TextColored(kColGrey, "%d", candidate);
	}
}

void CompareInt(const char* label, int candidate, int equipped)
{
	if (candidate == 0 && equipped == 0)
	{
		return;
	}
	ImGui::TableNextColumn();
	ImGui::Text("%s:", label);
	ImGui::SameLine();
	CompareValue(candidate, equipped);
}

void CompareStat(const char* label, int candBase, int wornBase, int candHeroic, int wornHeroic)
{
	if (candBase == 0 && wornBase == 0 && candHeroic == 0 && wornHeroic == 0)
	{
		return;
	}
	ImGui::TableNextColumn();
	ImGui::Text("%s:", label);
	ImGui::SameLine();
	CompareValue(candBase, wornBase);
	if (candHeroic != 0 || wornHeroic != 0)
	{
		int heroicDelta = candHeroic - wornHeroic;
		ImGui::SameLine();
		if (heroicDelta != 0)
		{
			ImGui::TextColored(kColHeroicSoft, "+%d (%+d)", candHeroic, heroicDelta);
		}
		else
		{
			ImGui::TextColored(kColHeroicSoft, "+%d", candHeroic);
		}
	}
}
} // namespace

const char* ItemTypeName(ItemDefinition* def)
{
	if (def->Type == ITEMTYPE_PACK)
	{
		int combine = def->Combine;
		if (combine >= 0 && combine < static_cast<int>(MAX_COMBINES) && szCombineTypes[combine])
		{
			return szCombineTypes[combine];
		}
		return nullptr;
	}
	if (def->Type == ITEMTYPE_BOOK)
	{
		return "Book";
	}
	int itemClass = def->ItemClass;
	if (itemClass >= 0 && itemClass < static_cast<int>(MAX_ITEMCLASSES) && szItemClasses[itemClass])
	{
		return szItemClasses[itemClass];
	}
	return nullptr;
}

int ItemRef::id() const          { return item ? item->GetID() : 0; }
const char* ItemRef::name() const { return item ? item->GetName() : ""; }
int ItemRef::icon() const        { return item ? item->GetIconID() : 0; }
int ItemRef::stack() const       { return item ? item->GetItemCount() : 0; }
int ItemRef::charges() const     { return item ? item->Charges : 0; }
bool ItemRef::isContainer() const { return item ? item->IsContainer() : false; }

int BagSlotCount()
{
	return InvSlot_LastBonusBagSlot - InvSlot_FirstBagSlot + 1;
}

const char* WornSlotCommandName(int wornSlot)
{
	if (wornSlot >= 0 && wornSlot < kWornCount)
	{
		return kWornCommandNames[wornSlot];
	}
	return "";
}

const char* WornSlotDisplayName(int wornSlot)
{
	if (wornSlot >= 0 && wornSlot < kWornCount)
	{
		return kWornDisplayNames[wornSlot];
	}
	return "";
}

const char* WornSlotBackgroundName(int wornSlot)
{
	if (wornSlot >= 0 && wornSlot < kWornCount)
	{
		return kWornBackgroundNames[wornSlot];
	}
	return nullptr;
}

ItemRef WornItem(int wornSlot)
{
	ItemRef ref;
	ref.location = ItemLoc_Worn;
	ref.slotId = wornSlot;
	if (PcProfile* profile = GetPcProfile())
	{
		ref.item = profile->GetInventorySlot(wornSlot);
	}
	return ref;
}

ItemRef BagItem(int packIndex)
{
	ItemRef ref;
	ref.location = ItemLoc_Bag;
	ref.slotId = packIndex;
	ref.bagSlot = -1;
	if (PcProfile* profile = GetPcProfile())
	{
		ref.item = profile->GetInventorySlot(InvSlot_FirstBagSlot + packIndex);
	}
	return ref;
}

std::vector<ItemRef> BagContents(int packIndex)
{
	std::vector<ItemRef> out;
	ItemRef bag = BagItem(packIndex);
	if (!bag.valid() || !bag.isContainer())
	{
		return out;
	}

	ItemContainer* contents = bag.item->GetChildItemContainer();
	if (!contents)
	{
		return out;
	}

	int size = contents->GetSize();
	out.reserve(size);
	for (int j = 0; j < size; ++j)
	{
		ItemRef ref;
		ref.item = contents->GetItem(j);
		ref.location = ItemLoc_Bag;
		ref.slotId = packIndex;
		ref.bagSlot = j;
		out.push_back(ref);
	}
	return out;
}

std::vector<ItemRef> GetBagContents()
{
	std::vector<ItemRef> out;
	for (int p = 0; p < BagSlotCount(); ++p)
	{
		for (ItemRef& ref : BagContents(p))
		{
			if (ref.valid())
			{
				out.push_back(ref);
			}
		}
	}
	return out;
}

std::vector<ItemRef> GetBank()
{
	std::vector<ItemRef> out;
	if (!pLocalPC)
	{
		return out;
	}
	ItemContainer& bank = pLocalPC->BankItems;
	int size = bank.GetSize();
	for (int i = 0; i < size; ++i)
	{
		ItemPtr top = bank.GetItem(i);
		if (!top)
		{
			continue;
		}
		if (top->IsContainer())
		{
			if (ItemContainer* contents = top->GetChildItemContainer())
			{
				int cs = contents->GetSize();
				for (int j = 0; j < cs; ++j)
				{
					ItemPtr it = contents->GetItem(j);
					if (it)
					{
						ItemRef r;
						r.item = it;
						r.location = ItemLoc_Bank;
						r.slotId = i;
						r.bagSlot = j;
						out.push_back(r);
					}
				}
			}
		}
		else
		{
			ItemRef r;
			r.item = top;
			r.location = ItemLoc_Bank;
			r.slotId = i;
			r.bagSlot = -1;
			out.push_back(r);
		}
	}
	return out;
}

bool IsClicky(const ItemRef& ref)
{
	if (!ref.valid())
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	return def && def->SpellData.GetSpellId(ItemSpellType_Clicky) > 0;
}

bool IsAugment(const ItemRef& ref)
{
	if (!ref.valid())
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	return def && def->AugType != 0;
}

bool ItemUsableByMe(const ItemRef& ref, bool checkLevel)
{
	if (!ref.valid())
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	bool consumable = def && (def->ItemClass == ItemClass_Food || def->ItemClass == ItemClass_Drink
		|| def->ItemClass == ItemClass_Combinable || def->ItemClass == ItemClass_Potion);
	bool canUse = pLocalPC && pLocalPC->CanUseItem(ref.item, checkLevel, false);
	return consumable || canUse;
}

bool ItemBelowReqLevel(const ItemRef& ref)
{
	if (!ref.valid())
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	return def && def->RequiredLevel > 0 && PlayerLevel() < def->RequiredLevel;
}

bool ItemAlreadyKnown(const ItemRef& ref)
{
	if (!ref.valid() || !pLocalPC)
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	if (!def)
	{
		return false;
	}
	// Spell scroll: the scroll effect names the spell it teaches. Disciplines /
	// skill tomes aren't held in the spellbook, so they degrade to unmarked here.
	int spellId = def->SpellData.GetSpellId(ItemSpellType_Scroll);
	if (spellId <= 0)
	{
		return false;
	}
	for (int i = 0; i < NUM_BOOK_SLOTS; ++i)
	{
		if (pLocalPC->GetSpellBook(i) == spellId)
		{
			return true;
		}
	}
	return false;
}

static bool SpellLearnableByMyClass(int spellId)
{
	if (!pLocalPC || spellId <= 0)
	{
		return false;
	}
	EQ_Spell* sp = mq::GetSpellByID(spellId);
	if (!sp)
	{
		return false;
	}
	int lvl = sp->GetSpellLevelNeeded(pLocalPC->GetClass());
	return lvl > 0 && lvl <= MAX_PC_LEVEL;
}

bool IsUseableGearItem(const ItemRef& ref)
{
	if (!ref.valid())
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	if (!def)
	{
		return false;
	}
	// Always drop food and drink.
	if (def->ItemClass == ItemClass_Food || def->ItemClass == ItemClass_Drink)
	{
		return false;
	}
	// Learnable spells / skills (spell scrolls, discipline tomes): keep only those your
	// class can actually learn, so other classes' scrolls/tomes are hidden. The
	// already-known indicator marks the ones already in your book.
	int scrollSpell = def->SpellData.GetSpellId(ItemSpellType_Scroll);
	if (def->ItemClass == ItemClass_Spell || scrollSpell > 0)
	{
		return SpellLearnableByMyClass(scrollSpell);
	}
	// Otherwise keep only equippable gear or clickies you can use; this drops
	// tradeskill / research clutter (runes, gems, quest mats). Required level is
	// surfaced as an icon, not filtered.
	bool equippable = def->EquipSlots != 0;
	if (!equippable && !IsClicky(ref))
	{
		return false;
	}
	return ItemUsableByMe(ref, false);
}

bool ItemFitsSlot(const ItemRef& ref, int wornSlot)
{
	if (!ref.valid())
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	return def && (def->EquipSlots & (1 << wornSlot)) != 0;
}

bool IsTwoHandedWeapon(const ItemRef& ref)
{
	if (!ref.valid())
	{
		return false;
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	if (!def)
	{
		return false;
	}
	switch (def->ItemClass)
	{
	case ItemClass_2HSlashing:
	case ItemClass_2HBlunt:
	case ItemClass_2HPiercing:
	case ItemClass_Martial:
		return true;
	default:
		return false;
	}
}

int CountInventory(const std::string& name)
{
	int total = 0;
	for (int s = 0; s < kWornCount; ++s)
	{
		ItemRef worn = WornItem(s);
		if (worn.valid() && ci_equals(worn.name(), name))
		{
			total += worn.stack() > 0 ? worn.stack() : 1;
		}
	}
	for (ItemRef& ref : GetBagContents())
	{
		if (ci_equals(ref.name(), name))
		{
			total += ref.stack() > 0 ? ref.stack() : 1;
		}
	}
	return total;
}

int CountBank(const std::string& name)
{
	int total = 0;
	for (ItemRef& ref : GetBank())
	{
		if (ci_equals(ref.name(), name))
		{
			total += ref.stack() > 0 ? ref.stack() : 1;
		}
	}
	return total;
}

std::vector<ItemRef> GetCompatibleItems(int wornSlot)
{
	std::vector<ItemRef> out;
	int slotMask = 1 << wornSlot;
	for (ItemRef& ref : GetBagContents())
	{
		ItemDefinition* def = ref.item->GetItemDefinition();
		if (!def || def->AugType != 0)
		{
			continue;
		}
		if ((def->EquipSlots & slotMask) == 0)
		{
			continue;
		}
		if (wornSlot == kSlotSecondary && IsTwoHandedWeapon(ref))
		{
			continue;
		}
		out.push_back(ref);
	}
	return out;
}

std::string ItemCompareKey(const ItemRef& ref)
{
	if (!ref.valid())
	{
		return "";
	}
	ItemDefinition* def = ref.item->GetItemDefinition();
	if (!def)
	{
		return "";
	}
	std::string key = def->Name ? def->Name : "";
	auto add = [&](int v) {
		key += '|';
		key += std::to_string(v);
	};
	add(AugmentedStat(ref, &ItemDefinition::AC));
	add(AugmentedStat(ref, &ItemDefinition::HP));
	add(AugmentedStat(ref, &ItemDefinition::Mana));
	add(AugmentedStat(ref, &ItemDefinition::Endurance));
	add(AugmentedStat(ref, &ItemDefinition::STR));
	add(AugmentedStat(ref, &ItemDefinition::STA));
	add(AugmentedStat(ref, &ItemDefinition::AGI));
	add(AugmentedStat(ref, &ItemDefinition::DEX));
	add(AugmentedStat(ref, &ItemDefinition::WIS));
	add(AugmentedStat(ref, &ItemDefinition::INT));
	add(AugmentedStat(ref, &ItemDefinition::CHA));
	add(AugmentedStat(ref, &ItemDefinition::HeroicSTR));
	add(AugmentedStat(ref, &ItemDefinition::HeroicSTA));
	add(AugmentedStat(ref, &ItemDefinition::HeroicAGI));
	add(AugmentedStat(ref, &ItemDefinition::HeroicDEX));
	add(AugmentedStat(ref, &ItemDefinition::HeroicWIS));
	add(AugmentedStat(ref, &ItemDefinition::HeroicINT));
	add(AugmentedStat(ref, &ItemDefinition::HeroicCHA));
	add(AugmentedStat(ref, &ItemDefinition::SvMagic));
	add(AugmentedStat(ref, &ItemDefinition::SvFire));
	add(AugmentedStat(ref, &ItemDefinition::SvCold));
	add(AugmentedStat(ref, &ItemDefinition::SvDisease));
	add(AugmentedStat(ref, &ItemDefinition::SvPoison));
	add(AugmentedStat(ref, &ItemDefinition::SvCorruption));
	add(AugmentedStat(ref, &ItemDefinition::Damage));
	add(static_cast<int>(def->Delay));
	add(AugmentedStat(ref, &ItemDefinition::Haste));
	return key;
}

ItemRef CursorItem()
{
	ItemRef ref;
	ref.location = ItemLoc_Cursor;
	if (PcProfile* profile = GetPcProfile())
	{
		ref.item = profile->GetInventorySlot(InvSlot_Cursor);
	}
	return ref;
}

bool CursorHasItem()
{
	return CursorItem().valid();
}

int GetFreeSlots()
{
	int free = 0;
	for (int p = 0; p < BagSlotCount(); ++p)
	{
		ItemRef bag = BagItem(p);
		if (!bag.valid() || !bag.isContainer())
		{
			continue;
		}
		ItemContainer* contents = bag.item->GetChildItemContainer();
		if (!contents)
		{
			continue;
		}
		int size = contents->GetSize();
		for (int j = 0; j < size; ++j)
		{
			if (!contents->GetItem(j))
			{
				++free;
			}
		}
	}
	return free;
}

int CoinAmount(int coinType)
{
	PcProfile* profile = GetPcProfile();
	if (!profile)
	{
		return 0;
	}
	switch (coinType)
	{
	case 0: return profile->GetPlatinum();
	case 1: return profile->GetGold();
	case 2: return profile->GetSilver();
	case 3: return profile->GetCopper();
	default: return 0;
	}
}

ItemRef FindItemById(int itemId)
{
	for (int s = 0; s < kWornCount; ++s)
	{
		ItemRef ref = WornItem(s);
		if (ref.valid() && ref.id() == itemId)
		{
			return ref;
		}
	}
	for (ItemRef& ref : GetBagContents())
	{
		if (ref.id() == itemId)
		{
			return ref;
		}
	}
	for (ItemRef& ref : GetBank())
	{
		if (ref.id() == itemId)
		{
			return ref;
		}
	}
	ItemRef cursor = CursorItem();
	if (cursor.valid() && cursor.id() == itemId)
	{
		return cursor;
	}
	return ItemRef{};
}

ItemRef FindBagItemByName(const std::string& name)
{
	for (ItemRef& ref : GetBagContents())
	{
		if (ci_equals(ref.name(), name))
		{
			return ref;
		}
	}
	return ItemRef{};
}

std::string PickupCommand(const ItemRef& ref)
{
	switch (ref.location)
	{
	case ItemLoc_Worn:
		return fmt::format("/itemnotify {} leftmouseup", WornSlotCommandName(ref.slotId));
	case ItemLoc_Bag:
		if (ref.bagSlot < 0)
		{
			return fmt::format("/itemnotify pack{} leftmouseup", ref.slotId + 1);
		}
		return fmt::format("/itemnotify in pack{} {} leftmouseup", ref.slotId + 1, ref.bagSlot + 1);
	case ItemLoc_Bank:
		if (ref.bagSlot < 0)
		{
			return fmt::format("/itemnotify bank{} leftmouseup", ref.slotId + 1);
		}
		return fmt::format("/itemnotify in bank{} {} leftmouseup", ref.slotId + 1, ref.bagSlot + 1);
	default:
		return "";
	}
}

void PickupOrDrop(const ItemRef& ref)
{
	std::string cmd = PickupCommand(ref);
	if (!cmd.empty())
	{
		EzCommand(cmd.c_str());
	}
}

void UseItem(const ItemRef& ref)
{
	if (ref.valid())
	{
		EzCommand(fmt::format("/useitem \"{}\"", ref.name()).c_str());
	}
}

void DrawItemIcon(IconHelper* icons, const ItemRef& ref, const DrawItemOptions& opts)
{
	float size = opts.size;
	ImVec2 origin = ImGui::GetCursorScreenPos();

	if (icons && opts.showBackground)
	{
		if (!opts.backgroundAnim || !icons->DrawStatusIcon(opts.backgroundAnim, static_cast<int>(size)))
		{
			icons->DrawStatusIcon("A_RecessedBox", static_cast<int>(size));
		}
		ImGui::SetCursorScreenPos(origin);
	}

	if (!ref.valid())
	{
		ImGui::PushID(ref.location * 10000 + ref.slotId * 100 + ref.bagSlot);
		if (ImGui::InvisibleButton("##empty", ImVec2(size, size)) && opts.handleLeftClick && CursorHasItem())
		{
			PickupOrDrop(ref);
		}
		ImGui::PopID();
		return;
	}

	CTextureAnimation* anim = icons ? icons->ItemIcon() : nullptr;
	if (!anim)
	{
		ImGui::Dummy(ImVec2(size, size));
		return;
	}

	MQColor tint(255, 255, 255, 255);
	if (opts.highlightUseable && !ItemUsableByMe(ref, true))
	{
		tint = MQColor(100, 100, 100, 255);
	}

	int cell = ref.icon() > 0 ? ref.icon() - 500 : 0;
	anim->SetCurCell(cell);
	imgui::DrawTextureAnimation(anim, CXSize(static_cast<int>(size), static_cast<int>(size)), tint, MQColor(0, 0, 0, 0));

	bool hovered = ImGui::IsItemHovered();

	ImDrawList* dl = ImGui::GetWindowDrawList();
	if (ref.stack() > 1)
	{
		char buf[16];
		sprintf_s(buf, "%d", ref.stack());
		ImVec2 ts = ImGui::CalcTextSize(buf);
		dl->AddText(ImVec2(origin.x + size - ts.x - 2.0f, origin.y + size - ts.y - 1.0f), IM_COL32(120, 230, 230, 255), buf);
	}
	if (ref.charges() > 0 && IsClicky(ref))
	{
		char buf[16];
		sprintf_s(buf, "%d", ref.charges());
		dl->AddText(ImVec2(origin.x + 2.0f, origin.y + 1.0f), IM_COL32(235, 220, 90, 255), buf);
	}
	if (opts.annotate)
	{
		float rx = origin.x + size - 1.0f;
		const float ty = origin.y + 1.0f;
		auto mark = [&](const char* glyph, ImU32 col) {
			ImVec2 ts = ImGui::CalcTextSize(glyph);
			rx -= ts.x;
			dl->AddText(ImVec2(rx + 1.0f, ty + 1.0f), IM_COL32(0, 0, 0, 200), glyph);
			dl->AddText(ImVec2(rx, ty), col, glyph);
			rx -= 2.0f;
		};
		if (ItemBelowReqLevel(ref))
		{
			mark(ICON_FA_LOCK, IM_COL32(230, 70, 70, 255));
		}
		if (ItemAlreadyKnown(ref))
		{
			mark(ICON_FA_CHECK, IM_COL32(70, 220, 90, 255));
		}
	}

	if (hovered)
	{
		RenderTooltip(icons, ref);
	}

	if (opts.handleLeftClick && ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		PickupOrDrop(ref);
	}
	else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (opts.handlePopInfo && io.KeyCtrl)
		{
			if (pItemDisplayManager)
			{
				pItemDisplayManager->ShowItem(ref.item);
			}
		}
		else if (opts.handlePopInfo && io.KeyShift)
		{
			PopInfoWindow(ref);
		}
		else if (opts.handleUse && !io.KeyShift)
		{
			UseItem(ref);
		}
	}
}

void RenderTooltip(IconHelper* icons, const ItemRef& ref)
{
	if (!ref.valid())
	{
		return;
	}
	ImGui::BeginTooltip();
	RenderItemInfo(icons, ref, false);
	ImGui::SeparatorText("Click Actions");
	ImGui::TextColored(kColGrey, "Left Click Pick Up item");
	ImGui::TextColored(kColGrey, "Right Click to use item");
	ImGui::TextColored(kColGrey, "Shift + Right Click to Pop Out Item Info");
	ImGui::TextColored(kColGrey, "Ctrl + Right Click to Inspect Item");
	ImGui::EndTooltip();
}

void RenderCompareTooltip(const ItemRef& candidate, const ItemRef& equipped)
{
	if (!candidate.valid())
	{
		return;
	}
	ItemDefinition* c = candidate.item->GetItemDefinition();
	if (!c)
	{
		return;
	}
	ItemDefinition* e = equipped.valid() ? equipped.item->GetItemDefinition() : nullptr;

	auto cand = [&](auto field) { return AugmentedStat(candidate, field); };
	auto worn = [&](auto field) { return AugmentedStat(equipped, field); };
	auto candFn = [&](int (ItemBase::*getter)() const) { return AugmentedStatFn(candidate, getter); };
	auto wornFn = [&](int (ItemBase::*getter)() const) { return AugmentedStatFn(equipped, getter); };

	auto setupCols = []() {
		ImGui::TableSetupColumn("##a", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableNextRow();
	};
	auto either = [&](auto field) { return cand(field) != 0 || worn(field) != 0; };

	bool hasBase = either(&ItemDefinition::AC) || either(&ItemDefinition::HP) || either(&ItemDefinition::Mana)
		|| either(&ItemDefinition::Endurance) || either(&ItemDefinition::HPRegen)
		|| either(&ItemDefinition::ManaRegen) || either(&ItemDefinition::EnduranceRegen);
	bool hasStats = either(&ItemDefinition::STR) || either(&ItemDefinition::STA) || either(&ItemDefinition::AGI)
		|| either(&ItemDefinition::DEX) || either(&ItemDefinition::WIS) || either(&ItemDefinition::INT)
		|| either(&ItemDefinition::CHA);
	bool hasResists = either(&ItemDefinition::SvMagic) || either(&ItemDefinition::SvFire) || either(&ItemDefinition::SvCold)
		|| either(&ItemDefinition::SvDisease) || either(&ItemDefinition::SvPoison) || either(&ItemDefinition::SvCorruption);

	ImGui::BeginTooltip();
	ImGui::TextColored(kColYellow, "%s", c->Name);
	ImGui::TextColored(kColGrey, "vs %s", e ? e->Name : "(empty)");
	ImGui::Separator();

	bool isWeapon = (c->Damage > 0 && c->Delay > 0) || (e && e->Damage > 0);
	if (isWeapon)
	{
		if (ImGui::BeginTable("##cmpdmg", 2, ImGuiTableFlags_None))
		{
			setupCols();
			CompareInt("Dmg", cand(&ItemDefinition::Damage), worn(&ItemDefinition::Damage));
			CompareInt("Dly", cand(&ItemDefinition::Delay), worn(&ItemDefinition::Delay));
			CompareInt("Haste", cand(&ItemDefinition::Haste), worn(&ItemDefinition::Haste));
			CompareInt("Dmg Shield", candFn(&ItemBase::GetDamShield), wornFn(&ItemBase::GetDamShield));
			CompareInt("DS Mit", candFn(&ItemBase::GetDamageShieldMitigation), wornFn(&ItemBase::GetDamageShieldMitigation));
			CompareInt("Avoidance", candFn(&ItemBase::GetAvoidance), wornFn(&ItemBase::GetAvoidance));
			CompareInt("DoT Shielding", candFn(&ItemBase::GetDoTShielding), wornFn(&ItemBase::GetDoTShielding));
			CompareInt("Accuracy", candFn(&ItemBase::GetAccuracy), wornFn(&ItemBase::GetAccuracy));
			CompareInt("Spell Shield", candFn(&ItemBase::GetSpellShield), wornFn(&ItemBase::GetSpellShield));
			CompareInt("Heal Amt", cand(&ItemDefinition::HealAmount), worn(&ItemDefinition::HealAmount));
			CompareInt("Spell Dmg", cand(&ItemDefinition::SpellDamage), worn(&ItemDefinition::SpellDamage));
			CompareInt("Stun Res", candFn(&ItemBase::GetStunResist), wornFn(&ItemBase::GetStunResist));
			CompareInt("Clairvoyance", cand(&ItemDefinition::Clairvoyance), worn(&ItemDefinition::Clairvoyance));
			ImGui::EndTable();
		}
	}

	if (hasBase)
	{
		ImGui::SeparatorText("Stats");
		if (ImGui::BeginTable("##cmpbase", 2, ImGuiTableFlags_None))
		{
			setupCols();
			CompareInt("AC", cand(&ItemDefinition::AC), worn(&ItemDefinition::AC));
			CompareInt("HPs", cand(&ItemDefinition::HP), worn(&ItemDefinition::HP));
			CompareInt("Mana", cand(&ItemDefinition::Mana), worn(&ItemDefinition::Mana));
			CompareInt("End", cand(&ItemDefinition::Endurance), worn(&ItemDefinition::Endurance));
			CompareInt("HP Regen", cand(&ItemDefinition::HPRegen), worn(&ItemDefinition::HPRegen));
			CompareInt("Mana Regen", cand(&ItemDefinition::ManaRegen), worn(&ItemDefinition::ManaRegen));
			CompareInt("End Regen", cand(&ItemDefinition::EnduranceRegen), worn(&ItemDefinition::EnduranceRegen));
			ImGui::EndTable();
		}
	}

	if (hasStats && ImGui::BeginTable("##cmpattr", 2, ImGuiTableFlags_None))
	{
		setupCols();
		CompareStat("STR", cand(&ItemDefinition::STR), worn(&ItemDefinition::STR), cand(&ItemDefinition::HeroicSTR), worn(&ItemDefinition::HeroicSTR));
		CompareStat("STA", cand(&ItemDefinition::STA), worn(&ItemDefinition::STA), cand(&ItemDefinition::HeroicSTA), worn(&ItemDefinition::HeroicSTA));
		CompareStat("AGI", cand(&ItemDefinition::AGI), worn(&ItemDefinition::AGI), cand(&ItemDefinition::HeroicAGI), worn(&ItemDefinition::HeroicAGI));
		CompareStat("DEX", cand(&ItemDefinition::DEX), worn(&ItemDefinition::DEX), cand(&ItemDefinition::HeroicDEX), worn(&ItemDefinition::HeroicDEX));
		CompareStat("WIS", cand(&ItemDefinition::WIS), worn(&ItemDefinition::WIS), cand(&ItemDefinition::HeroicWIS), worn(&ItemDefinition::HeroicWIS));
		CompareStat("INT", cand(&ItemDefinition::INT), worn(&ItemDefinition::INT), cand(&ItemDefinition::HeroicINT), worn(&ItemDefinition::HeroicINT));
		CompareStat("CHA", cand(&ItemDefinition::CHA), worn(&ItemDefinition::CHA), cand(&ItemDefinition::HeroicCHA), worn(&ItemDefinition::HeroicCHA));
		ImGui::EndTable();
	}

	if (hasResists)
	{
		ImGui::SeparatorText("Resists");
		if (ImGui::BeginTable("##cmpresist", 2, ImGuiTableFlags_None))
		{
			setupCols();
			CompareStat("Magic", cand(&ItemDefinition::SvMagic), worn(&ItemDefinition::SvMagic), candFn(&ItemBase::GetHeroicSvMagic), wornFn(&ItemBase::GetHeroicSvMagic));
			CompareStat("Fire", cand(&ItemDefinition::SvFire), worn(&ItemDefinition::SvFire), candFn(&ItemBase::GetHeroicSvFire), wornFn(&ItemBase::GetHeroicSvFire));
			CompareStat("Cold", cand(&ItemDefinition::SvCold), worn(&ItemDefinition::SvCold), candFn(&ItemBase::GetHeroicSvCold), wornFn(&ItemBase::GetHeroicSvCold));
			CompareStat("Disease", cand(&ItemDefinition::SvDisease), worn(&ItemDefinition::SvDisease), candFn(&ItemBase::GetHeroicSvDisease), wornFn(&ItemBase::GetHeroicSvDisease));
			CompareStat("Poison", cand(&ItemDefinition::SvPoison), worn(&ItemDefinition::SvPoison), candFn(&ItemBase::GetHeroicSvPoison), wornFn(&ItemBase::GetHeroicSvPoison));
			CompareStat("Corruption", cand(&ItemDefinition::SvCorruption), worn(&ItemDefinition::SvCorruption), candFn(&ItemBase::GetHeroicSvCorruption), wornFn(&ItemBase::GetHeroicSvCorruption));
			ImGui::EndTable();
		}
	}

	ImGui::Separator();
	ImGui::TextColored(kColGrey, "Hold Shift for full side-by-side comparison");
	ImGui::EndTooltip();
}

void RenderCompareSideBySide(IconHelper* icons, const ItemRef& candidate, const ItemRef& equipped)
{
	if (!candidate.valid())
	{
		return;
	}

	ImGui::BeginTooltip();
	ImGui::BeginChild("##cmpLeft", ImVec2(320.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
	ImGui::SeparatorText("Equipped");
	if (equipped.valid())
	{
		RenderItemInfo(icons, equipped, false);
	}
	else
	{
		ImGui::TextColored(kColGrey, "(empty slot)");
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##cmpRight", ImVec2(320.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
	ImGui::SeparatorText(candidate.name());
	RenderItemInfo(icons, candidate, false);
	ImGui::EndChild();
	ImGui::EndTooltip();
}

void PopInfoWindow(const ItemRef& ref)
{
	if (!ref.valid())
	{
		return;
	}
	std::vector<PoppedItem>& items = PoppedItems();
	for (const PoppedItem& entry : items)
	{
		if (entry.ref.id() == ref.id())
		{
			return;
		}
	}
	PoppedItem entry;
	entry.ref = ref;
	entry.spawnPos = ImGui::GetMousePos();
	items.push_back(entry);
}

void DrawPoppedInfoWindows(IconHelper* icons)
{
	std::vector<PoppedItem>& items = PoppedItems();
	for (size_t i = 0; i < items.size();)
	{
		PoppedItem& entry = items[i];
		ItemRef fresh = FindItemById(entry.ref.id());
		if (fresh.valid())
		{
			entry.ref = fresh;
		}
		if (!entry.ref.valid())
		{
			items.erase(items.begin() + i);
			continue;
		}

		bool open = true;
		char title[128];
		sprintf_s(title, "%s##ItemInfo_%d", entry.ref.name(), entry.ref.id());
		ImGui::SetNextWindowPos(ImVec2(entry.spawnPos.x - 30.0f, entry.spawnPos.y - 5.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(title, &open))
		{
			RenderItemInfo(icons, entry.ref, true);
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				open = false;
			}
		}
		ImGui::End();

		if (!open)
		{
			items.erase(items.begin() + i);
		}
		else
		{
			++i;
		}
	}

	DrawAugConfirmPopup();
}

void PulseAugInsert()
{
	AugInsertState& s = AugInsert();
	if (s.phase == 2)
	{
		ItemRef cursor = CursorItem();
		bool stillFits = s.target.valid() && cursor.valid()
			&& s.target.item->CanGemFitInSlot(cursor.item, s.slot, true, true) == 0;
		if (!stillFits || AugConfirmDialog())
		{
			s.phase = 0;
			return;
		}
		InsertAugment(s.target.item, s.slot);
		s.phase = 3;
		s.waitTicks = 0;
	}
	else if (s.phase == 3)
	{
		if (CXWnd* dlg = AugConfirmDialog())
		{
			if (CXWnd* yes = dlg->GetChildItem("CD_Yes_Button"))
			{
				SendWndClick2(yes, "leftmouseup");
			}
			s.phase = 0;
		}
		else if (++s.waitTicks > 8)
		{
			s.phase = 0;
		}
	}
}

void DrawCurrencyRow(IconHelper* icons, float iconSize, CoinPicker& picker, bool vertical)
{
	struct CoinKind { int icon; int type; const char* label; };
	static const CoinKind kCoins[] = {
		{ 644, 0, "Platinum" },
		{ 645, 1, "Gold" },
		{ 646, 2, "Silver" },
		{ 647, 3, "Copper" },
	};

	CTextureAnimation* anim = icons ? icons->ItemIcon() : nullptr;
	for (const CoinKind& coin : kCoins)
	{
		ImGui::PushID(coin.type);
		if (anim)
		{
			anim->SetCurCell(coin.icon - 500);
			imgui::DrawTextureAnimation(anim, CXSize(static_cast<int>(iconSize), static_cast<int>(iconSize)),
				MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));
			ImGui::SameLine();
		}

		int amount = CoinAmount(coin.type);
		std::string amountText = FormatThousands(amount);
		ImGui::TextColored(ImColor(0, 220, 220), "%s", amountText.c_str());
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s %s", amountText.c_str(), coin.label);
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			picker.show = true;
			picker.focus = false;
			picker.type = coin.type;
			picker.buf[0] = 0;
			ImVec2 mouse = ImGui::GetMousePos();
			picker.posX = mouse.x;
			picker.posY = mouse.y;
		}

		if (!vertical && coin.type != 3)
		{
			ImGui::SameLine();
		}
		ImGui::PopID();
	}
}

void DrawCoinQuantityWindow(CoinPicker& picker)
{
	if (!picker.show)
	{
		return;
	}

	static const char* kLabels[] = { "Plat", "Gold", "Silver", "Copper" };
	int maxQty = CoinAmount(picker.type);
	const char* label = (picker.type >= 0 && picker.type <= 3) ? kLabels[picker.type] : "Coin";

	bool open = true;
	ImGui::SetNextWindowPos(ImVec2(picker.posX, picker.posY), ImGuiCond_Appearing);
	if (ImGui::Begin("Quantity##MyUICoinQty", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter %s Qty", label);
		ImGui::Separator();

		if (!picker.focus)
		{
			ImGui::SetKeyboardFocusHere();
			picker.focus = true;
		}
		std::string hint = "Available: " + FormatThousands(maxQty);
		bool entered = ImGui::InputTextWithHint("##Qty", hint.c_str(), picker.buf, sizeof(picker.buf),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal);

		if (myui::StyledButton("Max##maxqty"))
		{
			sprintf_s(picker.buf, "%d", maxQty);
		}
		ImGui::SameLine();
		bool ok = myui::StyledButton("OK##qty") || entered;
		ImGui::SameLine();
		if (myui::StyledButton("Cancel##qty"))
		{
			picker.show = false;
			picker.focus = false;
		}

		if (ok)
		{
			int amount = mq::GetIntFromString(picker.buf, 0);
			if (amount > 0)
			{
				PickupCoin(picker.type, amount);
			}
			picker.show = false;
			picker.focus = false;
		}
	}
	ImGui::End();

	if (!open)
	{
		picker.show = false;
		picker.focus = false;
	}
}

void DrawDropZones()
{
	ImGui::SeparatorText("Inventory / Destroy Area");
	float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 4.0f;

	if (ImGui::BeginChild("AutoInvArea", ImVec2(halfWidth, 40.0f), ImGuiChildFlags_Borders))
	{
		ImGui::TextDisabled("Inventory Coin/Item");
	}
	ImGui::EndChild();
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		EzCommand("/autoinventory");
	}

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.0f, 0.0f, 1.0f));
	if (ImGui::BeginChild("DestroyArea", ImVec2(halfWidth, 40.0f), ImGuiChildFlags_Borders))
	{
		ImGui::TextDisabled("Destroy Item");
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered() && CursorHasItem() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		EzCommand("/destroy");
	}
}
} // namespace myui
