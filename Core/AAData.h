#pragma once

#include <string>
#include <vector>

struct AAEntry
{
	int groupId = 0;
	int index = 0;
	std::string name;
	int type = 0;
	int curRank = 0;
	int maxRank = 0;
	int nextCost = 0;
	int minLevel = 0;
	int spellId = -1;
	bool passive = true;
	int reuseSecs = 0;
	int prereqGroup = -1;
	int prereqRank = 0;
	bool canTrain = false;
	std::string category;
};

class AAData
{
public:
	void Update();
	void Rebuild();

	int Available() const { return m_available; }
	int Spent() const { return m_spent; }
	int Total() const { return m_available + m_spent; }

	void CategoryRanks(int type, int& cur, int& max) const;
	void AllRanks(int& cur, int& max) const;

	std::vector<const AAEntry*> ByType(int type) const;
	const AAEntry* Find(int groupId) const;
	std::string Description(int groupId) const;
	bool Ready(int groupId) const;

	const std::vector<AAEntry>& All() const { return m_entries; }

private:
	std::vector<AAEntry> m_entries;
	int m_available = 0;
	int m_spent = 0;
	int m_lastAvailable = -1;
	int m_lastSpent = -1;
	int m_curByType[6] = { 0 };
	int m_maxByType[6] = { 0 };
};
