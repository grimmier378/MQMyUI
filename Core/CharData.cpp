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

int ScrapeInvStat(const char* child)
{
	if (CXWnd* w = FindMQ2Window(fmt::format("InventoryWindow/{}", child).c_str()))
	{
		CXStr txt = w->GetWindowText();
		if (const char* s = txt.c_str())
		{
			return GetIntFromString(s, -1);
		}
	}
	return -1;
}

} // namespace

std::string InvControlTooltip(const char* child)
{
	if (CXWnd* w = FindMQ2Window(fmt::format("InventoryWindow/{}", child).c_str()))
	{
		CXStr tip = w->GetTooltip();
		if (tip.empty())
		{
			tip = w->GetXMLTooltip();
		}
		if (const char* s = tip.c_str())
		{
			return s;
		}
	}
	return "";
}

namespace
{

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
		m_snap.petPctHP = static_cast<int>(pPet->HPCurrent);
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

void CharData::RefreshStats()
{
	auto now = std::chrono::steady_clock::now();
	if (m_stats.valid && now < m_statsNext)
	{
		return;
	}
	m_statsNext = now + std::chrono::milliseconds(250);

	StatsSnapshot s{};
	if (GetGameState() != GAMESTATE_INGAME || !pLocalPC)
	{
		m_stats = s;
		return;
	}
	s.valid = true;

	s.hpCur   = GetCurHPS();
	s.hpMax   = GetMaxHPS();
	s.manaCur = GetCurMana();
	s.manaMax = GetMaxMana();
	s.endCur  = GetCurEndurance();
	s.endMax  = GetMaxEndurance();

	s.ac = ScrapeInvStat("IWS_CurrentArmorClass");
	if (s.ac < 0)
	{
		s.ac = 0;
	}
	s.attack = ScrapeInvStat("IWS_CurrentAttack");
	if (s.attack < 0)
	{
		s.attack = pLocalPC->AttackBonus;
	}
	s.hastePct = pLocalPC->TotalEffect(SPA_HASTE, true, 0, true, true);

	s.hpRegen   = pLocalPC->GetHPRegen(true, nullptr, true);
	s.manaRegen = pLocalPC->GetManaRegen(true, true);
	s.endRegen  = pLocalPC->GetEnduranceRegen(true, true);

	auto attr = [&](StatVal& sv, int value, const char* capChild, int heroic) {
		sv.value = value;
		sv.cap = ScrapeInvStat(capChild);
		sv.heroic = heroic;
	};
	attr(s.str,   pLocalPC->STR, "IWS_MaxStrength",     pLocalPC->HeroicSTRBonus);
	attr(s.sta,   pLocalPC->STA, "IWS_MaxStamina",      pLocalPC->HeroicSTABonus);
	attr(s.agi,   pLocalPC->AGI, "IWS_MaxAgility",      pLocalPC->HeroicAGIBonus);
	attr(s.dex,   pLocalPC->DEX, "IWS_MaxDexterity",    pLocalPC->HeroicDEXBonus);
	attr(s.wis,   pLocalPC->WIS, "IWS_MaxWisdom",       pLocalPC->HeroicWISBonus);
	attr(s.intel, pLocalPC->INT, "IWS_MaxIntelligence", pLocalPC->HeroicINTBonus);
	attr(s.cha,   pLocalPC->CHA, "IWS_MaxCharisma",     pLocalPC->HeroicCHABonus);

	auto resist = [&](StatVal& sv, int value, const char* capChild) {
		sv.value = value;
		sv.cap = ScrapeInvStat(capChild);
	};
	resist(s.rMagic,   pLocalPC->SaveMagic,      "IWS_MaxMagic");
	resist(s.rFire,    pLocalPC->SaveFire,       "IWS_MaxFire");
	resist(s.rCold,    pLocalPC->SaveCold,       "IWS_MaxCold");
	resist(s.rDisease, pLocalPC->SaveDisease,    "IWS_MaxDisease");
	resist(s.rPoison,  pLocalPC->SavePoison,     "IWS_MaxPoison");
	resist(s.rCorrupt, pLocalPC->SaveCorruption, "IWS_MaxCorruption");

	auto mod = [&](StatVal& sv, int value, int capIndex) {
		sv.value = value;
		sv.cap = GetModCap(capIndex);
	};
	mod(s.combatEffects, pLocalPC->CombatEffectsBonus,           HEROIC_MOD_COMBAT_EFFECTS);
	mod(s.spellShield,   pLocalPC->SpellShieldBonus,             HEROIC_MOD_SPELL_SHIELDING);
	mod(s.shielding,     pLocalPC->ShieldingBonus,               HEROIC_MOD_MELEE_SHIELDING);
	mod(s.dmgShield,     pLocalPC->DamageShieldBonus,            HEROIC_MOD_DAMAGE_SHIELDING);
	mod(s.dotShield,     pLocalPC->DoTShieldBonus,               HEROIC_MOD_DOT_SHIELDING);
	mod(s.dsMitigation,  pLocalPC->DamageShieldMitigationBonus,  HEROIC_MOD_DAMAGE_SHIELD_MITIG);
	mod(s.avoidance,     pLocalPC->AvoidanceBonus,               HEROIC_MOD_AVOIDANCE);
	mod(s.accuracy,      pLocalPC->AccuracyBonus,                HEROIC_MOD_ACCURACY);
	mod(s.stunResist,    pLocalPC->StunResistBonus,              HEROIC_MOD_STUN_RESIST);
	mod(s.strikeThrough, pLocalPC->StrikeThroughBonus,           HEROIC_MOD_STRIKETHROUGH);
	s.spellDamage = pLocalPC->SpellDamageBonus;

	static const char* kSkillBase[9] = {
		"Bash", "Backstab", "DragonPunch", "EagleStrike", "FlyingKick",
		"Kick", "RoundKick", "TigerClaw", "Frenzy",
	};
	for (int i = 0; i < 9; ++i)
	{
		int value = ScrapeInvStat(fmt::format("IWS_Current{}", kSkillBase[i]).c_str());
		if (value < 0)
		{
			value = pLocalPC->ItemSkillMinDamageMod[i] + pLocalPC->SkillMinDamageModBonus[i];
		}
		s.skillMod[i].value = value;
		s.skillMod[i].cap = ScrapeInvStat(fmt::format("IWS_Max{}", kSkillBase[i]).c_str());
	}

	s.weight = pLocalPC->CurrWeight;
	if (pEverQuest)
	{
		if (pLocalPlayer)
		{
			if (const char* cn = pEverQuest->GetClassDesc((EQClass)pLocalPlayer->GetClass()))
			{
				s.className = cn;
			}
		}
		if (char* dn = pEverQuest->GetDeityDesc(pLocalPC->GetDeity()))
		{
			s.deityName = dn;
		}
	}

	m_stats = s;
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
				m.classId    = sp->GetClass();
				m.standState = sp->StandState;
				m.velocity   = sp->SpeedRun;
				m.petId      = sp->PetID;
				m.inLOS      = m.isSelf || (pLocalPlayer && pLocalPlayer->CanSee(*sp));
				if (!m.isSelf && pLocalPlayer)
				{
					m.distance = Distance3DToSpawn(pLocalPlayer, sp);
				}
				m.pctHP      = m.isSelf ? m_snap.PctHP()   : static_cast<int>(sp->HPCurrent);
				m.pctMana    = m.isSelf ? m_snap.PctMana() : sp->ManaCurrent;
				m.pctEnd     = m.isSelf ? m_snap.PctEnd()  : sp->EnduranceCurrent;
				if (const char* cls = sp->GetClassThreeLetterCode())
				{
					m.classShort = cls;
				}
				if (PlayerClient* pPet = GetSpawnByID(sp->PetID))
				{
					m.petPctHP = static_cast<int>(pPet->HPCurrent);
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
			m.classId     = rm.nClass;
			m.groupNumber = rm.GroupNumber + 1;
			m.isLeader    = rm.RaidLeader || rm.GroupLeader;
#if IS_EMU_CLIENT
			m.present     = rm.IsInZone;
#else
			m.present     = (GetSpawnByName(rm.Name) != nullptr);
#endif
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
				if (!m.isSelf && pLocalPlayer)
				{
					m.distance = Distance3DToSpawn(pLocalPlayer, sp);
				}
				m.pctHP      = m.isSelf ? m_snap.PctHP() : static_cast<int>(sp->HPCurrent);
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
