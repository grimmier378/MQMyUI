#include "AAData.h"
#include "SpellInfo.h"

#include <mq/Plugin.h>

#include <algorithm>
#include <cassert>
#include <unordered_map>

namespace
{
std::string CleanDescription(const char* raw)
{
	std::string s = raw ? raw : "";
	size_t pos = 0;
	while ((pos = s.find("<BR>", pos)) != std::string::npos)
	{
		s.replace(pos, 4, "\n");
		pos += 1;
	}
	pos = 0;
	while ((pos = s.find("<br>", pos)) != std::string::npos)
	{
		s.replace(pos, 4, "\n");
		pos += 1;
	}
	return s;
}
} // namespace

void AAData::Update()
{
	PcProfile* prof = GetPcProfile();
	if (!prof)
	{
		return;
	}

	if (m_entries.empty() || prof->AAPoints != m_lastAvailable || prof->AAPointsSpent != m_lastSpent)
	{
		Rebuild();
	}
}

void AAData::Rebuild()
{
	m_entries.clear();
	for (int i = 0; i < 6; ++i)
	{
		m_curByType[i] = 0;
		m_maxByType[i] = 0;
	}

	PcProfile* prof = GetPcProfile();
	if (!prof || !pLocalPC || !pAltAdvManager)
	{
		return;
	}

	m_available = prof->AAPoints;
	m_spent = prof->AAPointsSpent;
	m_lastAvailable = m_available;
	m_lastSpent = m_spent;

	HashTable<CAltAbilityData*>* tbl = pAltAdvManager->abilities;
	if (!tbl)
	{
		return;
	}

	std::unordered_map<int, CAltAbilityData*> baseRow;
	std::unordered_map<int, int> maxRankByGroup;

	CAltAbilityData** pp = tbl->WalkFirst();
	while (pp)
	{
		CAltAbilityData* ab = *pp;
		if (ab && pAltAdvManager->CanSeeAbility(pLocalPC, ab))
		{
			int g = ab->GroupID;
			auto it = baseRow.find(g);
			if (it == baseRow.end() || ab->CurrentRank < it->second->CurrentRank)
			{
				baseRow[g] = ab;
			}
			if (ab->MaxRank > maxRankByGroup[g])
			{
				maxRankByGroup[g] = ab->MaxRank;
			}
		}
		pp = tbl->WalkNext(pp);
	}

	m_entries.reserve(baseRow.size());
	for (const auto& [groupId, base] : baseRow)
	{
		AAEntry e;
		e.groupId  = groupId;
		e.index    = base->Index;
		e.name     = base->GetNameString() ? base->GetNameString() : "";
		e.type     = base->Type;
		e.maxRank  = maxRankByGroup[groupId];
		e.category = base->GetCategoryString() ? base->GetCategoryString() : "";

		CAltAbilityData* owned = pAltAdvManager->GetOwnedAbilityFromGroupID(pLocalPC, groupId);
		e.curRank = owned ? owned->CurrentRank : 0;

		CAltAbilityData* rep = owned ? owned : base;
		e.spellId = rep->SpellID;
		e.passive = (rep->SpellID == -1);
		if (!e.passive)
		{
			e.reuseSecs = static_cast<int>(pAltAdvManager->GetCalculatedTimer(pLocalPC, rep));
		}

		CAltAbilityData* next = nullptr;
		if (e.curRank < e.maxRank)
		{
			next = owned ? pAltAdvManager->GetAAById(owned->NextGroupAbilityId) : base;
		}

		if (next)
		{
			e.nextCost = next->Cost;
			e.minLevel = next->MinLevel;
			e.canTrain = pAltAdvManager->CanTrainAbility(pLocalPC, next);
			if (next->RequiredGroups.GetLength() > 0)
			{
				e.prereqGroup = next->RequiredGroups[0];
				if (next->RequiredGroupLevels.GetLength() > 0)
				{
					e.prereqRank = next->RequiredGroupLevels[0];
				}
			}
		}

		if (e.type >= 1 && e.type <= 5)
		{
			m_curByType[e.type] += e.curRank;
			m_maxByType[e.type] += e.maxRank;
		}

		m_entries.push_back(std::move(e));
	}

	std::sort(m_entries.begin(), m_entries.end(), [](const AAEntry& a, const AAEntry& b) {
		return ci_string_compare(a.name, b.name) < 0;
	});
}

void AAData::CategoryRanks(int type, int& cur, int& max) const
{
	if (type >= 1 && type <= 5)
	{
		cur = m_curByType[type];
		max = m_maxByType[type];
	}
	else
	{
		cur = 0;
		max = 0;
	}
}

void AAData::AllRanks(int& cur, int& max) const
{
	cur = 0;
	max = 0;
	for (int i = 1; i <= 5; ++i)
	{
		cur += m_curByType[i];
		max += m_maxByType[i];
	}
}

std::vector<const AAEntry*> AAData::ByType(int type) const
{
	std::vector<const AAEntry*> out;
	for (const AAEntry& e : m_entries)
	{
		if (e.type == type)
		{
			out.push_back(&e);
		}
	}
	return out;
}

const AAEntry* AAData::Find(int groupId) const
{
	for (const AAEntry& e : m_entries)
	{
		if (e.groupId == groupId)
		{
			return &e;
		}
	}
	return nullptr;
}

CAltAbilityData* AAData::ResolveAbility(int groupId) const
{
	if (!pAltAdvManager || !pLocalPC)
	{
		return nullptr;
	}

	CAltAbilityData* ab = pAltAdvManager->GetOwnedAbilityFromGroupID(pLocalPC, groupId);
	if (!ab)
	{
		const AAEntry* e = Find(groupId);
		if (e)
		{
			ab = pAltAdvManager->GetAAById(e->index);
		}
	}
	return ab;
}

std::string AAData::Description(int groupId) const
{
	CAltAbilityData* ab = ResolveAbility(groupId);
	if (!ab)
	{
		return "";
	}

	std::string out = CleanDescription(ab->GetDescriptionString());

	if (ab->SpellID != -1)
	{
		std::string eff = myui::SpellEffectText(ab->SpellID);
		if (!eff.empty())
		{
			if (!out.empty())
			{
				out += "\n\n";
			}
			out += eff;
		}
	}

	return out;
}

bool AAData::Ready(int groupId) const
{
	CAltAbilityData* ab = ResolveAbility(groupId);
	if (!ab)
	{
		return false;
	}

	int refresh = 0;
	int timer = 0;
	return pAltAdvManager->IsAbilityReady(pLocalPC, ab, &refresh, &timer);
}
