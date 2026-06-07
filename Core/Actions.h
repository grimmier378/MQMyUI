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

void PulseActions();
}
