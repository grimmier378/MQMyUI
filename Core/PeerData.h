#pragma once

#include "CharData.h"

#include <chrono>
#include <string>
#include <vector>

namespace myui
{
enum GroupRoleBit
{
	kRoleMainTank   = 0x1,
	kRoleMainAssist = 0x2,
	kRolePuller     = 0x4,
};

struct ItemCount
{
	int inventory = 0;
	int bank = 0;
};

struct PeerRecord
{
	std::string server;
	std::string character;
	std::string maskedName; // RACE_CLASS_LEVEL code for anonymized display
	std::string classShort;
	int level = 0;

	int curHP = 0, maxHP = 0;
	int curMana = 0, maxMana = 0;
	int curEnd = 0, maxEnd = 0;
	int zoneId = 0;
	std::string zoneName;
	int pctAggro = 0;
	int standState = 0;
	float velocity = 0.0f;
	int petId = 0;
	int petPctHP = 0;
	std::string petName;
	std::string groupLeader;
	int roleMask = 0;

	int aaSpent = 0;
	int aaTotal = 0;
	int aaAvailable = 0;
	float pctAAExp = 0.0f;
	float pctExp = 0.0f;
	float pctAir = 100.0f;
	std::string state;
	std::string allocation;

	std::vector<BuffInfo> buffs;

	std::string storageId;
	std::string host;

	bool hasVitals = false;
	bool hasAA = false;
	bool hasBuffs = false;

	std::chrono::steady_clock::time_point lastSeen;

	int PctHP()   const { return maxHP   > 0 ? (curHP   * 100) / maxHP   : 0; }
	int PctMana() const { return maxMana > 0 ? (curMana * 100) / maxMana : 0; }
	int PctEnd()  const { return maxEnd  > 0 ? (curEnd  * 100) / maxEnd  : 0; }
};

inline std::string PeerKey(const std::string& server, const std::string& character)
{
	return server + "|" + character;
}
} // namespace myui
