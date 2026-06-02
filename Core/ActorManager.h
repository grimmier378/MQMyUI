#pragma once

#include "PeerData.h"

#include <functional>

#include <mq/api/ActorAPI.h>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CharData;
class ChatBridge;
class SettingsStore;

class ActorManager
{
public:
	void Init(CharData* charData, ChatBridge* chat, SettingsStore* settings, const std::string& server, const std::string& character);
	void Shutdown();
	void Pulse();

	bool IsValid() const { return m_valid; }

	void SendCommand(const std::string& toServer, const std::string& toChar, const std::string& action, int arg = 0, const std::string& argStr = "");
	void BroadcastReload(const std::string& targetList);

	void SetReloadHandler(std::function<void()> fn) { m_onReload = std::move(fn); }

	const std::unordered_map<std::string, myui::PeerRecord>& Peers() const { return m_peers; }
	const myui::PeerRecord* FindPeerByName(const std::string& name) const;

	const std::vector<std::string>& TrackedItems() const { return m_trackedItems; }
	void AddTrackedItem(const std::string& name);
	void RemoveTrackedItem(const std::string& name);
	const std::map<std::string, myui::ItemCount>* CountsForItem(const std::string& itemName) const;
	void SwitchTo(const std::string& character);

private:
	void OnReceive(const std::shared_ptr<mq::postoffice::Message>& msg);
	void PublishVitals();
	void PublishAA();
	void PublishBuffs();
	void PublishInventory(const std::string& removeItem = "");
	void PruneStale();
	void ExecuteCommand(const std::string& action, const std::string& fromChar, int arg, const std::string& argStr);
	bool ReloadTargetsSelf(const std::string& targetList) const;
	void LoadTrackedList();
	void SaveTrackedList();

	mq::postoffice::DropboxAPI m_dropbox{};
	bool m_valid = false;
	CharData* m_char = nullptr;
	ChatBridge* m_chat = nullptr;
	SettingsStore* m_settings = nullptr;
	std::function<void()> m_onReload;
	std::string m_server;
	std::string m_character;

	std::unordered_map<std::string, myui::PeerRecord> m_peers;
	std::vector<std::string> m_trackedItems;
	std::map<std::string, std::map<std::string, myui::ItemCount>> m_itemCounts;

	std::chrono::steady_clock::time_point m_nextVitals;
	std::chrono::steady_clock::time_point m_nextAA;
	std::chrono::steady_clock::time_point m_nextBuffs;
	std::chrono::steady_clock::time_point m_nextInventory;
	std::chrono::steady_clock::time_point m_nextPrune;
};
