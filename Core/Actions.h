#pragma once

#include <string>
#include <vector>

namespace myui
{
void GiveItemTo(int spawnId);
void SwapToWornSlot(const std::string& pickupCommand, const std::string& dropCommand, bool autoInventory,
	const std::string& preUnequipCommand = "", int swapItemId = 0);
void PickupCoin(int coinType, int amount);
void StartBulkTrade(const std::vector<std::string>& itemNames);
void KickFromGroup(int spawnId, const std::string& name);

struct SpellSetStep
{
	int slot = 0;
	int spellId = 0; // 0 = clear the slot
};
void StartSpellSetLoad(const std::vector<SpellSetStep>& steps);
bool SpellSetLoadActive();

void PulseActions();
}
