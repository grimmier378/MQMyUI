#pragma once

#include <string>
#include <vector>

struct BuffInfo
{
	int slot = 0;
	bool isEmpty = false;
	int spellId = 0;
	int iconId = 0;
	int durationMs = 0;
	std::string name;
	std::string caster;
	bool beneficial = true;
};

struct CharSnapshot
{
	bool valid = false;

	std::string name;
	std::string server;
	std::string classShort;
	int classId = 0;
	int level   = 0;

	int curHP = 0,   maxHP = 0;
	int curMana = 0, maxMana = 0;
	int curEnd = 0,  maxEnd = 0;

	bool isCaster = false;
	bool inCombat = false;
	int  standState = 0;
	int  petId = -1;
	int  petPctHP = 0;
	std::string petName;
	int  zoneId = 0;
	std::string zoneName;
	int  pctAggro = 0;
	float velocity = 0.0f;

	std::string groupLeader;
	int roleMask = 0;

	int aaSpent = 0;
	int aaTotal = 0;
	int aaAvailable = 0;
	float pctAAExp = 0.0f;
	float pctExp = 0.0f;
	float pctAir = 100.0f;
	std::string combatState;
	std::string allocation;

	int PctHP()   const { return maxHP   > 0 ? (curHP   * 100) / maxHP   : 0; }
	int PctMana() const { return maxMana > 0 ? (curMana * 100) / maxMana : 0; }
	int PctEnd()  const { return maxEnd  > 0 ? (curEnd  * 100) / maxEnd  : 0; }
};

struct GroupMemberSnap
{
	std::string name;
	std::string classShort;
	int level = 0;
	bool isSelf = false;
	bool present = false;
	bool offline = false;
	bool isLeader = false;
	bool inLOS = false;
	int roleMask = 0;
	int spawnId = 0;
	int petId = 0;
	int petPctHP = 0;
	int standState = 0;
	float velocity = 0.0f;
	float distance = 0.0f;
	int pctHP = 0;
	int pctMana = 0;
	int pctEnd = 0;
	int groupNumber = 0;
};

class CharData
{
public:
	void Refresh();
	const CharSnapshot& Get() const { return m_snap; }
	bool IsIngame() const { return m_snap.valid; }

	void RefreshBuffs();
	void RefreshSongs();
	const std::vector<BuffInfo>& Buffs() const { return m_buffs; }
	const std::vector<BuffInfo>& Songs() const { return m_songs; }

	const std::vector<GroupMemberSnap>& Group() const { return m_group; }
	const std::vector<GroupMemberSnap>& Raid() const { return m_raid; }
	bool InRaid() const { return m_inRaid; }

private:
	void RefreshGroupRaid();

	CharSnapshot m_snap;
	std::vector<BuffInfo> m_buffs;
	std::vector<BuffInfo> m_songs;
	std::vector<GroupMemberSnap> m_group;
	std::vector<GroupMemberSnap> m_raid;
	bool m_inRaid = false;
};
