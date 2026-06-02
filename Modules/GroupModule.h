#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/CharData.h"

#include <mq/Plugin.h>

#include <string>

struct GroupRowData;

class GroupModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Group"; }

	void OnRenderGUI() override;

private:
	void DrawGroupWindow();
	void DrawRaidWindow();
	void DrawRaidGroupBlock(int groupNumber, const std::vector<const GroupMemberSnap*>& members);
	void DrawRaidDummyTile();
	void DrawMemberRow(const GroupRowData& row);
	void DrawVitalBars(const GroupRowData& row);
	void TargetMember(const GroupRowData& row);
	void DrawMemberContextMenu(const GroupRowData& row);
	void DrawMemberTooltip(const GroupRowData& row);

	GroupRowData BuildRow(const GroupMemberSnap& snap) const;

	std::string m_curWindow = "Group";
};
