#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/SettingsStore.h"

#include <mq/Plugin.h>

#include <map>
#include <set>
#include <string>
#include <vector>

class CopyModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Copy"; }

	void OnRenderGUI() override;
	void OnPulse() override;

private:
	struct PendingWrite
	{
		std::string module;
		std::string name;
		std::string type;
		std::string value;
	};

	void RefreshChars();
	void RefreshRows();
	void QueueCopy(const std::string& module, const std::string& name, const std::string& type, const std::string& value);
	void RequestApply();

	int PrimaryDst() const;

	std::vector<CharIdent>  m_chars;
	std::vector<SettingRow> m_src;
	std::vector<SettingRow> m_dst;

	// Rebuilt in RefreshRows alongside m_src/m_dst; pointers index into those vectors.
	std::map<std::string, std::map<std::string, const SettingRow*>> m_srcRowIdx;
	std::map<std::string, std::map<std::string, const SettingRow*>> m_dstRowIdx;
	std::map<std::string, std::set<std::string>>                    m_byModule;

	int  m_srcIdx = -1;
	std::set<int> m_dstSel;
	bool m_wasOpen = false;

	std::vector<PendingWrite> m_queue;
	std::vector<CharIdent> m_pendingDsts;
	bool m_applyPending = false;
};
