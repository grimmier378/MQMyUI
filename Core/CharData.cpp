#include "CharData.h"
#include "PeerData.h"

#include <mq/Plugin.h>

#include <cmath>

namespace
{
const char* CombatStateString()
{
	switch (GetCombatState())
	{
	case eCombatState_Combat:   return "COMBAT";
	case eCombatState_Debuff:   return "DEBUFFED";
	case eCombatState_Timer:    return "COOLDOWN";
	case eCombatState_Regen:    return "RESTING";
	case eCombatState_Standing:
	default:                    return "ACTIVE";
	}
}

int SelfRoleMask(CGroupMember* self)
{
	int mask = 0;
	if (!self)
	{
		return mask;
	}
	if (self->IsMainTank())
	{
		mask |= myui::kRoleMainTank;
	}
	if (self->IsMainAssist())
	{
		mask |= myui::kRoleMainAssist;
	}
	if (self->IsPuller())
	{
		mask |= myui::kRolePuller;
	}
	return mask;
}
} // namespace

void CharData::Refresh()
{
	m_snap = CharSnapshot{};

	if (GetGameState() != GAMESTATE_INGAME || !pLocalPC || !pLocalPlayer)
	{
		return;
	}

	m_snap.valid = true;
	m_snap.name = pLocalPC->Name;

	if (const char* server = GetServerShortName())
	{
		m_snap.server = server;
	}

	m_snap.level   = pLocalPC->GetLevel();
	m_snap.classId = pLocalPlayer->GetClass();
	if (const char* cls = pLocalPlayer->GetClassThreeLetterCode())
	{
		m_snap.classShort = cls;
	}

	m_snap.curHP   = GetCurHPS();
	m_snap.maxHP   = GetMaxHPS();
	m_snap.curMana = GetCurMana();
	m_snap.maxMana = GetMaxMana();
	m_snap.curEnd  = GetCurEndurance();
	m_snap.maxEnd  = GetMaxEndurance();

	m_snap.isCaster   = m_snap.maxMana > 0;
	m_snap.inCombat   = pEverQuestInfo && pEverQuestInfo->bAutoAttack;
	m_snap.standState = pLocalPlayer->StandState;
	m_snap.velocity   = pLocalPlayer->SpeedRun;
	m_snap.petId      = pLocalPlayer->PetID;

	if (PlayerClient* pPet = GetSpawnByID(pLocalPlayer->PetID))
	{
		m_snap.petName  = pPet->DisplayedName;
		m_snap.petPctHP = pPet->HPCurrent;
	}

	if (pZoneInfo)
	{
		m_snap.zoneId = pZoneInfo->ZoneID;
	}
	if (const char* zn = GetFullZone(m_snap.zoneId))
	{
		m_snap.zoneName = zn;
	}

	if (pAggroInfo)
	{
		m_snap.pctAggro = pAggroInfo->aggroData[AD_Player].AggroPct;
	}

	if (PcProfile* pProfile = GetPcProfile())
	{
		m_snap.aaAvailable = pProfile->AAPoints;
		m_snap.aaSpent     = pProfile->AAPointsSpent;
		m_snap.aaTotal     = pProfile->AAPointsSpent + pProfile->AAPoints;
	}
	m_snap.pctAAExp = (float)pLocalPC->AAExp / EXP_TO_PCT_RATIO;
	m_snap.pctExp   = (float)pLocalPC->Exp / EXP_TO_PCT_RATIO;

	if (int maxAir = pLocalPC->GetMaxAirSupply())
	{
		m_snap.pctAir = (float)(pLocalPC->GetAirSupply() * 100) / maxAir;
	}

	if (pAAWnd)
	{
		if (CXWnd* percentCountWnd = pAAWnd->GetChildItem("AAW_PercentCount"))
		{
			CXStr txt = percentCountWnd->GetWindowText();
			if (const char* s = txt.c_str())
			{
				m_snap.allocation = s;
			}
		}
	}

	m_snap.combatState = CombatStateString();

	if (pLocalPC->Group)
	{
		if (CGroupMember* leader = pLocalPC->Group->GetGroupLeader())
		{
			m_snap.groupLeader = leader->GetName();
		}
		m_snap.roleMask = SelfRoleMask(pLocalPC->Group->GetGroupMember(pLocalPlayer));
	}

	RefreshGroupRaid();
}

void CharData::RefreshGroupRaid()
{
	m_group.clear();
	m_raid.clear();
	m_inRaid = false;

	if (pLocalPC && pLocalPC->Group)
	{
		const char* leaderName = nullptr;
		if (CGroupMember* leader = pLocalPC->Group->GetGroupLeader())
		{
			leaderName = leader->GetName();
		}

		for (int i = 0; i < MAX_GROUP_SIZE; ++i)
		{
			CGroupMember* pMember = pLocalPC->Group->GetGroupMember(i);
			if (!pMember || !pMember->GetName() || pMember->GetName()[0] == 0)
			{
				continue;
			}

			GroupMemberSnap m;
			m.name    = pMember->GetName();
			m.level   = pMember->GetLevel();
			m.offline = pMember->IsOffline();
			m.isSelf  = (i == 0);
			m.isLeader = leaderName && ci_equals(m.name, leaderName);
			if (pMember->IsMainTank())
			{
				m.roleMask |= myui::kRoleMainTank;
			}
			if (pMember->IsMainAssist())
			{
				m.roleMask |= myui::kRoleMainAssist;
			}
			if (pMember->IsPuller())
			{
				m.roleMask |= myui::kRolePuller;
			}

			if (PlayerClient* sp = pMember->GetPlayer())
			{
				m.present    = true;
				m.spawnId    = sp->SpawnID;
				m.standState = sp->StandState;
				m.velocity   = sp->SpeedRun;
				m.petId      = sp->PetID;
				m.inLOS      = m.isSelf || (pLocalPlayer && pLocalPlayer->CanSee(*sp));
				if (!m.isSelf && pLocalPlayer)
				{
					float dx = sp->X - pLocalPlayer->X;
					float dy = sp->Y - pLocalPlayer->Y;
					m.distance = sqrtf(dx * dx + dy * dy);
				}
				m.pctHP      = m.isSelf ? m_snap.PctHP()   : sp->HPCurrent;
				m.pctMana    = m.isSelf ? m_snap.PctMana() : sp->ManaCurrent;
				m.pctEnd     = m.isSelf ? m_snap.PctEnd()  : sp->EnduranceCurrent;
				if (const char* cls = sp->GetClassThreeLetterCode())
				{
					m.classShort = cls;
				}
				if (PlayerClient* pPet = GetSpawnByID(sp->PetID))
				{
					m.petPctHP = pPet->HPCurrent;
				}
			}
			else if (m.isSelf)
			{
				m.present = true;
				m.pctHP   = m_snap.PctHP();
				m.classShort = m_snap.classShort;
			}

			m_group.push_back(std::move(m));
		}
	}

	if (pRaid && pRaid->RaidMemberCount > 0)
	{
		m_inRaid = true;
		auto& raidMembers = pRaid->RaidMember;
		for (int i = 0; i < MAX_RAID_SIZE; ++i)
		{
			const RaidMember& rm = raidMembers[i];
			if (rm.Name[0] == 0)
			{
				continue;
			}

			GroupMemberSnap m;
			m.name        = rm.Name;
			m.level       = rm.nLevel;
			m.groupNumber = rm.GroupNumber + 1;
			m.isLeader    = rm.RaidLeader || rm.GroupLeader;
			m.present     = rm.IsInZone;
			m.isSelf      = ci_equals(rm.Name, pLocalPC->Name);
			if (rm.RaidMainAssist)
			{
				m.roleMask |= myui::kRoleMainAssist;
			}
			if (pEverQuest)
			{
				if (const char* cls = pEverQuest->GetClassThreeLetterCode((EQClass)rm.nClass))
				{
					m.classShort = cls;
				}
			}
			if (PlayerClient* sp = GetSpawnByName(rm.Name))
			{
				m.spawnId    = sp->SpawnID;
				m.standState = sp->StandState;
				m.velocity   = sp->SpeedRun;
				m.petId      = sp->PetID;
				m.inLOS      = m.isSelf || (pLocalPlayer && pLocalPlayer->CanSee(*sp));
				m.pctHP      = m.isSelf ? m_snap.PctHP() : sp->HPCurrent;
			}

			m_raid.push_back(std::move(m));
		}
	}
}

void CharData::RefreshBuffs()
{
	m_buffs.clear();
	if (!pBuffWnd)
	{
		return;
	}

	int maxBuffs = GetCharMaxBuffSlots();
	int wndMax = pBuffWnd->GetMaxBuffs();
	if (maxBuffs > wndMax)
	{
		maxBuffs = wndMax;
	}
	for (int i = 0; i < maxBuffs; ++i)
	{
		BuffInfo b;
		b.slot = i;

		auto buffInfo = pBuffWnd->GetBuffInfo(i);
		EQ_Spell* spell = buffInfo.GetSpell();
		if (!spell)
		{
			b.isEmpty = true;
			m_buffs.push_back(std::move(b));
			continue;
		}

		b.spellId    = spell->ID;
		b.iconId     = spell->SpellIcon;
		b.durationMs = buffInfo.GetBuffTimer();
		b.name       = spell->Name;
		if (const char* caster = buffInfo.GetCaster())
		{
			b.caster = caster;
		}
		b.beneficial = spell->IsBeneficialSpell();
		m_buffs.push_back(std::move(b));
	}
}

void CharData::RefreshSongs()
{
	m_songs.clear();
	if (!pSongWnd)
	{
		return;
	}

	int slot = 0;
	for (const auto& buffInfo : pSongWnd->GetBuffRange())
	{
		BuffInfo b;
		b.slot = slot++;

		EQ_Spell* spell = buffInfo.GetSpell();
		if (!spell)
		{
			b.isEmpty = true;
			m_songs.push_back(std::move(b));
			continue;
		}

		b.spellId    = spell->ID;
		b.iconId     = spell->SpellIcon;
		b.durationMs = buffInfo.GetBuffTimer();
		b.name       = spell->Name;
		if (const char* caster = buffInfo.GetCaster())
		{
			b.caster = caster;
		}
		b.beneficial = spell->IsBeneficialSpell();
		m_songs.push_back(std::move(b));
	}
}
