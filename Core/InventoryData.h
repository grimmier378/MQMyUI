#pragma once

#include <mq/Plugin.h>

#include <string>
#include <vector>

class IconHelper;

namespace myui
{
enum ItemLocationKind
{
	ItemLoc_Worn = 0,
	ItemLoc_Bag,
	ItemLoc_Bank,
	ItemLoc_Cursor,
};

struct ItemRef
{
	ItemPtr item;
	int location = ItemLoc_Worn;
	int slotId   = -1;
	int bagSlot  = -1;

	bool valid() const { return static_cast<bool>(item); }
	int  id() const;
	const char* name() const;
	int  icon() const;
	int  stack() const;
	int  charges() const;
	bool isContainer() const;
};

constexpr int kSlotPrimary = 13;
constexpr int kSlotSecondary = 14;

const ImVec4 kItemNameColor = ImVec4(242.0f / 255.0f, 140.0f / 255.0f, 40.0f / 255.0f, 1.0f);

int BagSlotCount();
const char* WornSlotCommandName(int wornSlot);
const char* WornSlotDisplayName(int wornSlot);
const char* WornSlotBackgroundName(int wornSlot);

ItemRef WornItem(int wornSlot);
ItemRef BagItem(int packIndex);
std::vector<ItemRef> BagContents(int packIndex);
std::vector<ItemRef> GetBagContents();
std::vector<ItemRef> GetBank();
std::vector<ItemRef> GetCompatibleItems(int wornSlot);
std::string ItemCompareKey(const ItemRef& ref);
const char* ItemTypeName(ItemDefinition* def);
ItemRef CursorItem();
bool CursorHasItem();
int GetFreeSlots();
int CoinAmount(int coinType);
ItemRef FindItemById(int itemId);
ItemRef FindBagItemByName(const std::string& name);

bool IsClicky(const ItemRef& ref);
bool IsAugment(const ItemRef& ref);
bool IsTwoHandedWeapon(const ItemRef& ref);
bool ItemFitsSlot(const ItemRef& ref, int wornSlot);

// Usable by the local character. checkLevel=false ignores the required-level gate
// (class/race/deity only) so it can drive the "Useable Only" filter while the
// level is surfaced separately. Consumables (food/drink/combine/potion) always pass.
bool ItemUsableByMe(const ItemRef& ref, bool checkLevel);
// True when the item has a required level the character hasn't reached.
bool ItemBelowReqLevel(const ItemRef& ref);
// True when the item is a spell scroll teaching a spell already in the spellbook.
bool ItemAlreadyKnown(const ItemRef& ref);
// Item belongs in the "Useable Only" gear view: usable by my class/race/deity AND
// either equippable or clicky. Drops food/drink and non-equippable no-click clutter
// (tradeskill/research mats: runes, gems, ...). Required level is NOT enforced here.
bool IsUseableGearItem(const ItemRef& ref);

int CountInventory(const std::string& name);
int CountBank(const std::string& name);

std::string PickupCommand(const ItemRef& ref);
void PickupOrDrop(const ItemRef& ref);
void UseItem(const ItemRef& ref);

struct DrawItemOptions
{
	float size = 40.0f;
	bool  showBackground = true;
	bool  highlightUseable = false;
	bool  annotate = false; // draw top-right level-not-met / already-known markers
	bool  handleLeftClick = true;
	bool  handleUse = true;
	bool  handlePopInfo = true;
	const char* backgroundAnim = nullptr;
};

void DrawItemIcon(IconHelper* icons, const ItemRef& ref, const DrawItemOptions& opts);
void RenderTooltip(IconHelper* icons, const ItemRef& ref);
void RenderCompareTooltip(const ItemRef& candidate, const ItemRef& equipped);
void RenderCompareSideBySide(IconHelper* icons, const ItemRef& candidate, const ItemRef& equipped);
void PopInfoWindow(const ItemRef& ref);
void DrawPoppedInfoWindows(IconHelper* icons);
void PulseAugInsert();

struct CoinPicker
{
	bool  show = false;
	bool  focus = false;
	int   type = 0;
	char  buf[32] = { 0 };
	float posX = 0.0f;
	float posY = 0.0f;
};

void DrawCurrencyRow(IconHelper* icons, float iconSize, CoinPicker& picker, bool vertical = false);
void DrawCoinQuantityWindow(CoinPicker& picker);
void DrawDropZones();
} // namespace myui
