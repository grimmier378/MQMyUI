#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/CharData.h"

#include <mq/Plugin.h>

#include <string>

struct GroupRowData;
struct BarStyle;

class GroupModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Group"; }

	void OnRenderGUI() override;
	void OnZone() override;

private:
	void DrawGroupWindow();
	void DrawRaidWindow();
	void DrawRaidGroupBlock(int groupNumber, const std::vector<const GroupMemberSnap*>& members);
	void DrawRaidDummyTile();
	void DrawMemberRow(const GroupRowData& row);
	void DrawVitalBars(const GroupRowData& row);
	void ApplyOutOfZoneColor(BarStyle& bar, bool outOfZone) const;
	void TargetMember(const GroupRowData& row);
	void DrawMemberContextMenu(const GroupRowData& row);
	void DrawMemberTooltip(const GroupRowData& row);
	void DrawCommandButtons(const std::vector<GroupMemberSnap>& members, const char* scopeLabel, bool& followActive);
	void SendToMembers(const std::vector<GroupMemberSnap>& members, const std::string& action, int arg = 0, const std::string& argStr = "");

	GroupRowData BuildRow(const GroupMemberSnap& snap) const;

	std::string m_curWindow = "Group";
	bool m_groupFollow = false;
	bool m_raidFollow = false;
};
