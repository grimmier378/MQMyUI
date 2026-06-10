#include "ActorManager.h"
#include "CharData.h"
#include "ChatBridge.h"
#include "SettingsStore.h"
#include "InventoryData.h"
#include "UiHelpers.h"

#include "../Proto/myui.pb.h"

#include <mq/Plugin.h>

namespace
{
const char* kMailbox = "myui";
constexpr auto kVitalsInterval    = std::chrono::seconds(1);
constexpr auto kAAInterval        = std::chrono::seconds(2);
constexpr auto kBuffsInterval     = std::chrono::seconds(3);
constexpr auto kInventoryInterval = std::chrono::seconds(4);
constexpr auto kPruneInterval     = std::chrono::seconds(5);
constexpr auto kStaleAfter        = std::chrono::seconds(60);
}

void ActorManager::Init(CharData* charData, ChatBridge* chat, SettingsStore* settings, const std::string& server, const std::string& character,
	const std::string& storageId, const std::string& host)
{
	m_char = charData;
	m_chat = chat;
	m_settings = settings;
	m_server = server;
	m_character = character;
	m_storageId = storageId;
	m_host = host;
	m_peers.clear();
	m_seenPeers.clear();
	m_itemCounts.clear();
	LoadTrackedList();

	if (m_dropbox.Dropbox)
	{
		m_dropbox.Remove();
	}

	m_dropbox = mq::postoffice::AddActor(kMailbox,
		[this](const std::shared_ptr<mq::postoffice::Message>& msg) { OnReceive(msg); });

	m_valid = m_dropbox.Dropbox != nullptr;
	if (!m_valid && m_chat)
	{
		m_chat->Print("MyUI", "Actor mailbox 'myui' failed to register; multi-character features disabled.");
	}

	auto now = std::chrono::steady_clock::now();
	m_nextVitals = now;
	m_nextAA = now;
	m_nextBuffs = now;
	m_nextInventory = now;
	m_nextPrune = now + kPruneInterval;
}

void ActorManager::Shutdown()
{
	if (m_dropbox.Dropbox)
	{
		m_dropbox.Remove();
	}
	m_dropbox = mq::postoffice::DropboxAPI{};
	m_valid = false;
	m_peers.clear();
	m_seenPeers.clear();
}

void ActorManager::Pulse()
{
	if (!m_valid || !m_char)
	{
		return;
	}

	auto now = std::chrono::steady_clock::now();

	if (now >= m_nextVitals)
	{
		PublishVitals();
		m_nextVitals = now + kVitalsInterval;
	}
	if (now >= m_nextAA)
	{
		PublishAA();
		m_nextAA = now + kAAInterval;
	}
	if (now >= m_nextBuffs)
	{
		PublishBuffs();
		m_nextBuffs = now + kBuffsInterval;
	}
	if (now >= m_nextInventory)
	{
		PublishInventory();
		m_nextInventory = now + kInventoryInterval;
	}
	if (now >= m_nextPrune)
	{
		PruneStale();
		m_nextPrune = now + kPruneInterval;
	}
}

void ActorManager::PublishVitals()
{
	const CharSnapshot& c = m_char->Get();
	if (!c.valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* v = env.mutable_vitals();
	v->set_server(c.server);
	v->set_character(c.name);
	v->set_race(c.raceId);
	v->set_class_short(c.classShort);
	v->set_level(c.level);
	v->set_cur_hp(c.curHP);
	v->set_max_hp(c.maxHP);
	v->set_cur_mana(c.curMana);
	v->set_max_mana(c.maxMana);
	v->set_cur_end(c.curEnd);
	v->set_max_end(c.maxEnd);
	v->set_zone_id(c.zoneId);
	v->set_zone_name(c.zoneName);
	v->set_pct_aggro(c.pctAggro);
	v->set_stand_state(c.standState);
	v->set_velocity(c.velocity);
	v->set_pet_id(c.petId);
	v->set_pet_pct_hp(c.petPctHP);
	v->set_pet_name(c.petName);
	v->set_group_leader(c.groupLeader);
	v->set_role_mask(c.roleMask);
	v->set_storage_id(m_storageId);
	v->set_host(m_host);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
}

void ActorManager::PublishAA()
{
	const CharSnapshot& c = m_char->Get();
	if (!c.valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* a = env.mutable_aa();
	a->set_server(c.server);
	a->set_character(c.name);
	a->set_level(c.level);
	a->set_aa_spent(c.aaSpent);
	a->set_aa_total(c.aaTotal);
	a->set_aa_available(c.aaAvailable);
	a->set_pct_aa_exp(c.pctAAExp);
	a->set_pct_exp(c.pctExp);
	a->set_pct_air(c.pctAir);
	a->set_state(c.combatState);
	a->set_allocation(c.allocation);
	a->set_group_leader(c.groupLeader);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
}

void ActorManager::PublishBuffs()
{
	const CharSnapshot& c = m_char->Get();
	if (!c.valid)
	{
		return;
	}

	m_char->RefreshBuffs();

	mq::proto::myui::MyUIEnvelope env;
	auto* list = env.mutable_buffs();
	list->set_server(c.server);
	list->set_character(c.name);

	for (const BuffInfo& b : m_char->Buffs())
	{
		auto* e = list->add_buffs();
		e->set_slot(b.slot);
		e->set_is_empty(b.isEmpty);
		if (b.isEmpty)
		{
			continue;
		}
		e->set_name(b.name);
		e->set_spell_id(b.spellId);
		e->set_duration_ms(b.durationMs);
		e->set_icon_id(b.iconId);
		e->set_beneficial(b.beneficial);
		e->set_caster(b.caster);
	}

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
}

void ActorManager::PublishInventory(const std::string& removeItem)
{
	if (!m_valid || !m_char)
	{
		return;
	}
	const CharSnapshot& c = m_char->Get();
	if (!c.valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* snap = env.mutable_inventory();
	snap->set_server(c.server);
	snap->set_character(c.name);
	if (!removeItem.empty())
	{
		snap->set_remove(removeItem);
	}

	auto& selfCounts = m_itemCounts;
	std::vector<myui::ItemRef> invSnap = myui::GetInventorySnapshot();
	std::vector<myui::ItemRef> bankSnap = myui::GetBank();
	for (const std::string& itemName : m_trackedItems)
	{
		int inventory = myui::CountIn(invSnap, itemName);
		int bank = myui::CountIn(bankSnap, itemName);
		selfCounts[itemName][c.name] = myui::ItemCount{ inventory, bank };

		auto* entry = snap->add_items();
		entry->set_name(itemName);
		entry->set_inventory(inventory);
		entry->set_bank(bank);

		snap->add_tracking(itemName);
	}

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
}

void ActorManager::OnReceive(const std::shared_ptr<mq::postoffice::Message>& msg)
{
	if (!msg || !msg->Payload)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	if (!env.ParseFromString(*msg->Payload))
	{
		return;
	}

	auto now = std::chrono::steady_clock::now();

	switch (env.payload_case())
	{
	case mq::proto::myui::MyUIEnvelope::PayloadCase::kVitals:
	{
		const auto& v = env.vitals();
		if (ci_equals(v.character(), m_character))
		{
			return;
		}

		const std::string peerKey = myui::PeerKey(v.server(), v.character());
		const bool isNewPeer = (m_peers.find(peerKey) == m_peers.end());
		auto& p = m_peers[peerKey];
		p.server      = v.server();
		p.character   = v.character();
		p.classShort  = v.class_short();
		p.level       = v.level();
		// Masking is local-only: build the anonymized code here from the peer's
		// real race/class/level (the wire only ever carries real names).
		p.maskedName  = myui::MaskedCode(v.race(), v.class_short(), v.level());
		p.curHP       = v.cur_hp();
		p.maxHP       = v.max_hp();
		p.curMana     = v.cur_mana();
		p.maxMana     = v.max_mana();
		p.curEnd      = v.cur_end();
		p.maxEnd      = v.max_end();
		p.zoneId      = v.zone_id();
		p.zoneName    = v.zone_name();
		p.pctAggro    = v.pct_aggro();
		p.standState  = v.stand_state();
		p.velocity    = v.velocity();
		p.petId       = v.pet_id();
		p.petPctHP    = v.pet_pct_hp();
		p.petName     = v.pet_name();
		p.groupLeader = v.group_leader();
		p.roleMask    = v.role_mask();
		p.storageId   = v.storage_id();
		p.host        = v.host();
		p.hasVitals   = true;
		p.lastSeen    = now;

		// First time we have seen this peer: pull its settings into our local mirror, unless it
		// shares our SQLite file (same-store), in which case the rows are already present.
		if (isNewPeer && m_seenPeers.insert(peerKey).second && !IsSameStore(p))
		{
			SendSettingsRequest(v.server(), v.character());
		}
		break;
	}
	case mq::proto::myui::MyUIEnvelope::PayloadCase::kAa:
	{
		const auto& a = env.aa();
		if (ci_equals(a.character(), m_character))
		{
			return;
		}

		auto& p = m_peers[myui::PeerKey(a.server(), a.character())];
		p.server      = a.server();
		p.character   = a.character();
		p.level       = a.level();
		p.aaSpent     = a.aa_spent();
		p.aaTotal     = a.aa_total();
		p.aaAvailable = a.aa_available();
		p.pctAAExp    = a.pct_aa_exp();
		p.pctExp      = a.pct_exp();
		p.pctAir      = a.pct_air();
		p.state       = a.state();
		p.allocation  = a.allocation();
		p.groupLeader = a.group_leader();
		p.hasAA       = true;
		p.lastSeen    = now;
		break;
	}
	case mq::proto::myui::MyUIEnvelope::PayloadCase::kBuffs:
	{
		const auto& list = env.buffs();
		if (ci_equals(list.character(), m_character))
		{
			return;
		}

		auto& p = m_peers[myui::PeerKey(list.server(), list.character())];
		p.server    = list.server();
		p.character = list.character();
		p.buffs.clear();
		p.buffs.reserve(list.buffs_size());
		for (const auto& e : list.buffs())
		{
			BuffInfo b;
			b.slot       = e.slot();
			b.isEmpty    = e.is_empty();
			b.name       = e.name();
			b.spellId    = e.spell_id();
			b.durationMs = e.duration_ms();
			b.iconId     = e.icon_id();
			b.beneficial = e.beneficial();
			b.caster     = e.caster();
			p.buffs.push_back(std::move(b));
		}
		p.hasBuffs = true;
		p.lastSeen = now;
		break;
	}
	case mq::proto::myui::MyUIEnvelope::PayloadCase::kCommand:
	{
		const auto& cmd = env.command();
		if (ci_equals(cmd.to_char(), m_character))
		{
			ExecuteCommand(cmd.action(), cmd.from_char(), cmd.arg(), cmd.arg_str());
		}
		else if (cmd.to_char().empty() && ci_equals(cmd.action(), "ReloadSettings") && ReloadTargetsSelf(cmd.arg_str()))
		{
			if (m_onReload)
			{
				m_onReload();
			}
		}
		break;
	}
	case mq::proto::myui::MyUIEnvelope::PayloadCase::kInventory:
	{
		const auto& snap = env.inventory();
		if (ci_equals(snap.character(), m_character))
		{
			return;
		}

		if (!snap.remove().empty())
		{
			const std::string& removed = snap.remove();
			m_itemCounts.erase(removed);
			for (auto it = m_trackedItems.begin(); it != m_trackedItems.end(); ++it)
			{
				if (ci_equals(*it, removed))
				{
					m_trackedItems.erase(it);
					SaveTrackedList();
					break;
				}
			}
		}

		for (const auto& entry : snap.items())
		{
			m_itemCounts[entry.name()][snap.character()] = myui::ItemCount{ entry.inventory(), entry.bank() };
		}

		bool listChanged = false;
		for (const auto& tracked : snap.tracking())
		{
			bool found = false;
			for (const std::string& existing : m_trackedItems)
			{
				if (ci_equals(existing, tracked))
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				m_trackedItems.push_back(tracked);
				listChanged = true;
			}
		}
		if (listChanged)
		{
			SaveTrackedList();
		}
		break;
	}
	case mq::proto::myui::MyUIEnvelope::PayloadCase::kSettings:
	{
		const auto& s = env.settings();
		if (ci_equals(s.from_char(), m_character))
		{
			return;
		}

		std::vector<SettingRow> rows;
		rows.reserve(s.rows_size());
		for (const auto& r : s.rows())
		{
			rows.push_back(SettingRow{ r.module(), r.name(), r.type(), r.value() });
		}

		const bool addressedToUs = ci_equals(s.to_char(), m_character)
			&& (s.to_server().empty() || ci_equals(s.to_server(), m_server));

		switch (s.kind())
		{
		case mq::proto::myui::SettingsSync::PUSH:
			if (!s.to_char().empty())
			{
				// Directed PUSH: we are the authoritative owner -> apply to our own DB, reply with results.
				if (addressedToUs && m_onSettingsApply)
				{
					std::vector<SettingRow> current = m_onSettingsApply(m_server, m_character, rows, s.full());
					SendSettingsReply(s.from_server(), s.from_char(), current, false);
				}
			}
			else
			{
				// Undirected PUSH = "mirror me". Skip same-store senders (shared file already current).
				auto it = m_peers.find(myui::PeerKey(s.from_server(), s.from_char()));
				const bool sameStore = (it != m_peers.end()) && IsSameStore(it->second);
				if (!sameStore && m_onSettingsMirror)
				{
					m_onSettingsMirror(s.from_server(), s.from_char(), rows, s.full());
				}
			}
			break;

		case mq::proto::myui::SettingsSync::REPLY:
			if (m_onSettingsMirror)
			{
				m_onSettingsMirror(s.from_server(), s.from_char(), rows, s.full());
			}
			break;

		case mq::proto::myui::SettingsSync::REQUEST:
			if (addressedToUs && m_settings)
			{
				SendSettingsReply(s.from_server(), s.from_char(),
					m_settings->GetAllSettings(m_server, m_character), true);
			}
			break;

		default:
			break;
		}
		break;
	}
	default:
		break;
	}
}

void ActorManager::ExecuteCommand(const std::string& action, const std::string& fromChar, int arg, const std::string& argStr)
{
	if (ci_equals(action, "Switch"))
	{
		EzCommand("/foreground");
	}
	else if (ci_equals(action, "ComeTo"))
	{
		EzCommand(fmt::format("/nav spawn =\"{}\" dist={} lineofsight=on", fromChar, arg > 0 ? arg : 10).c_str());
	}
	else if (ci_equals(action, "ComeLoc"))
	{
		if (!argStr.empty() && pZoneInfo && static_cast<int>(pZoneInfo->ZoneID) == arg)
		{
			EzCommand(fmt::format("/nav locxyz {}", argStr).c_str());
		}
	}
	else if (ci_equals(action, "FollowMe"))
	{
		if (PlayerClient* sp = GetSpawnByName(fromChar.c_str()))
		{
			EzCommand("/nav stop");
			EzCommand(fmt::format("/afollow spawn {}", sp->SpawnID).c_str());
		}
	}
	else if (ci_equals(action, "FollowStop"))
	{
		EzCommand("/afollow off");
	}
	else if (ci_equals(action, "MakeLeader"))
	{
		EzCommand(fmt::format("/makeleader {}", fromChar).c_str());
	}
	else if (ci_equals(action, "Less"))
	{
		EzCommand("/notify AAWindow AAW_LessExpButton leftmouseup");
	}
	else if (ci_equals(action, "More"))
	{
		EzCommand("/notify AAWindow AAW_MoreExpButton leftmouseup");
	}
	else if (ci_equals(action, "Min"))
	{
		EzCommand("/alt on 0");
	}
	else if (ci_equals(action, "Mid"))
	{
		EzCommand("/alt on 50");
	}
	else if (ci_equals(action, "Max"))
	{
		EzCommand("/alt on 100");
	}
	else if (ci_equals(action, "RemoveBuff"))
	{
		EzCommand(fmt::format("/removebuff \"{}\"", argStr).c_str());
	}
	else if (ci_equals(action, "BlockBuff"))
	{
		EzCommand(fmt::format("/blockspell add me {}", arg).c_str());
	}
	else if (ci_equals(action, "ReloadSettings"))
	{
		if (m_onReload)
		{
			m_onReload();
		}
	}
}

void ActorManager::SendCommand(const std::string& toServer, const std::string& toChar, const std::string& action, int arg, const std::string& argStr)
{
	if (!m_valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* cmd = env.mutable_command();
	cmd->set_from_server(m_server);
	cmd->set_from_char(m_character);
	cmd->set_to_char(toChar);
	cmd->set_action(action);
	cmd->set_arg(arg);
	cmd->set_arg_str(argStr);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	addr.Character = toChar;
	if (!toServer.empty())
	{
		addr.Server = toServer;
	}
	m_dropbox.Post(addr, env);
}

void ActorManager::BroadcastReload(const std::string& targetList)
{
	if (!m_valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* cmd = env.mutable_command();
	cmd->set_from_server(m_server);
	cmd->set_from_char(m_character);
	cmd->set_to_char("");
	cmd->set_action("ReloadSettings");
	cmd->set_arg_str(targetList);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
}

namespace
{
void FillSettingRows(mq::proto::myui::SettingsSync* sync, const std::vector<SettingRow>& rows)
{
	for (const SettingRow& r : rows)
	{
		auto* row = sync->add_rows();
		row->set_module(r.module);
		row->set_name(r.name);
		row->set_type(r.type);
		row->set_value(r.value);
	}
}
}

void ActorManager::SendSettingsPush(const std::string& toServer, const std::string& toChar, const std::vector<SettingRow>& rows, bool full)
{
	if (!m_valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* s = env.mutable_settings();
	s->set_from_server(m_server);
	s->set_from_char(m_character);
	s->set_to_server(toServer);
	s->set_to_char(toChar);
	s->set_kind(mq::proto::myui::SettingsSync::PUSH);
	s->set_full(full);
	FillSettingRows(s, rows);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	addr.Character = toChar;
	if (!toServer.empty())
	{
		addr.Server = toServer;
	}
	m_dropbox.Post(addr, env);
}

void ActorManager::BroadcastSettingsPush(const std::vector<SettingRow>& rows)
{
	if (!m_valid || rows.empty())
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* s = env.mutable_settings();
	s->set_from_server(m_server);
	s->set_from_char(m_character);
	s->set_to_server("");
	s->set_to_char("");
	s->set_kind(mq::proto::myui::SettingsSync::PUSH);
	s->set_full(false);
	FillSettingRows(s, rows);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
}

void ActorManager::SendSettingsRequest(const std::string& toServer, const std::string& toChar)
{
	if (!m_valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* s = env.mutable_settings();
	s->set_from_server(m_server);
	s->set_from_char(m_character);
	s->set_to_server(toServer);
	s->set_to_char(toChar);
	s->set_kind(mq::proto::myui::SettingsSync::REQUEST);
	s->set_full(false);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	addr.Character = toChar;
	if (!toServer.empty())
	{
		addr.Server = toServer;
	}
	m_dropbox.Post(addr, env);
}

void ActorManager::SendSettingsReply(const std::string& toServer, const std::string& toChar, const std::vector<SettingRow>& rows, bool full)
{
	if (!m_valid)
	{
		return;
	}

	mq::proto::myui::MyUIEnvelope env;
	auto* s = env.mutable_settings();
	s->set_from_server(m_server);
	s->set_from_char(m_character);
	s->set_to_server(toServer);
	s->set_to_char(toChar);
	s->set_kind(mq::proto::myui::SettingsSync::REPLY);
	s->set_full(full);
	FillSettingRows(s, rows);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	addr.Character = toChar;
	if (!toServer.empty())
	{
		addr.Server = toServer;
	}
	m_dropbox.Post(addr, env);
}

bool ActorManager::ReloadTargetsSelf(const std::string& targetList) const
{
	for (const std::string& entry : mq::split(targetList, ';'))
	{
		if (entry.empty())
		{
			continue;
		}
		std::vector<std::string> parts = mq::split(entry, '|');
		const std::string& srv = parts.size() > 0 ? parts[0] : std::string();
		const std::string& chr = parts.size() > 1 ? parts[1] : std::string();
		if (ci_equals(chr, m_character) && (srv.empty() || ci_equals(srv, m_server)))
		{
			return true;
		}
	}
	return false;
}

void ActorManager::PruneStale()
{
	auto now = std::chrono::steady_clock::now();
	for (auto it = m_peers.begin(); it != m_peers.end();)
	{
		if (now - it->second.lastSeen > kStaleAfter)
		{
			const std::string staleChar = it->second.character;
			for (auto& [itemName, charCounts] : m_itemCounts)
			{
				charCounts.erase(staleChar);
			}
			m_seenPeers.erase(it->first);
			it = m_peers.erase(it);
		}
		else
		{
			++it;
		}
	}
}

const myui::PeerRecord* ActorManager::FindPeerByName(const std::string& name) const
{
	for (const auto& [key, peer] : m_peers)
	{
		if (ci_equals(peer.character, name))
		{
			return &peer;
		}
	}
	return nullptr;
}

void ActorManager::LoadTrackedList()
{
	m_trackedItems.clear();
	if (!m_settings)
	{
		return;
	}
	std::string raw = m_settings->GetGlobal("iTrack", "TrackedList", "");
	for (const std::string& token : mq::split(raw, '|'))
	{
		if (!token.empty())
		{
			m_trackedItems.push_back(token);
		}
	}
}

void ActorManager::SaveTrackedList()
{
	if (!m_settings)
	{
		return;
	}
	m_settings->SetGlobal("iTrack", "TrackedList", mq::join(m_trackedItems, "|"));
}

void ActorManager::AddTrackedItem(const std::string& name)
{
	if (name.empty())
	{
		return;
	}
	for (const std::string& existing : m_trackedItems)
	{
		if (ci_equals(existing, name))
		{
			return;
		}
	}
	m_trackedItems.push_back(name);
	SaveTrackedList();
	PublishInventory();
}

void ActorManager::RemoveTrackedItem(const std::string& name)
{
	for (auto it = m_trackedItems.begin(); it != m_trackedItems.end(); ++it)
	{
		if (ci_equals(*it, name))
		{
			m_trackedItems.erase(it);
			break;
		}
	}
	m_itemCounts.erase(name);
	SaveTrackedList();
	PublishInventory(name);
}

const std::map<std::string, myui::ItemCount>* ActorManager::CountsForItem(const std::string& itemName) const
{
	auto it = m_itemCounts.find(itemName);
	return it != m_itemCounts.end() ? &it->second : nullptr;
}

void ActorManager::SwitchTo(const std::string& character)
{
	SendCommand("", character, "Switch");
}
