#include "GroupModule.h"

#include "../Core/UiConfig.h"
#include "../Core/BarEngine.h"
#include "../Core/ThemeManager.h"
#include "../Core/ActorManager.h"
#include "../Core/PeerData.h"
#include "../Core/UiHelpers.h"
#include "../Core/IconHelper.h"
#include "../Core/Actions.h"
#include "../Core/Widgets.h"

#include "imgui/fonts/IconsFontAwesome.h"
#include "imgui/fonts/IconsMaterialDesign.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

struct GroupRowData
{
	std::string name;
	std::string classShort;
	std::string zoneName;
	std::string server;
	int level = 0;
	bool isSelf = false;
	bool present = false;
	bool offline = false;
	bool outOfZone = false;
	bool hasData = false;
	bool isLeader = false;
	bool inLOS = false;
	int roleMask = 0;
	int pctHP = 0;
	int pctMana = 0;
	int pctEnd = 0;
	int curHP = 0, maxHP = 0;
	int curMana = 0, maxMana = 0;
	int curEnd = 0, maxEnd = 0;
	bool realValues = false;
	bool manaClass = false;
	bool hasManaEnd = true;
	int petPctHP = 0;
	bool hasPet = false;
	std::string petName;
	float distance = 0.0f;
	float velocity = 0.0f;
	int standState = 0;
	int spawnId = 0;
	int petId = 0;
};

GroupRowData GroupModule::BuildRow(const GroupMemberSnap& snap) const
{
	GroupRowData row;
	row.name       = snap.name;
	row.classShort = snap.classShort;
	row.level      = snap.level;
	row.isSelf     = snap.isSelf;
	row.offline    = snap.offline;
	row.present    = snap.present;
	row.isLeader   = snap.isLeader;
	row.roleMask   = snap.roleMask;
	row.spawnId    = snap.spawnId;
	row.petId      = snap.petId;
	row.distance   = snap.distance;
	row.velocity   = snap.velocity;
	row.standState = snap.standState;
	row.inLOS      = snap.inLOS;

	const CharSnapshot& self = m_ctx.Char->Get();

	if (snap.isSelf)
	{
		row.hasData   = true;
		row.realValues = true;
		row.server    = self.server;
		row.zoneName  = self.zoneName;
		row.classShort = self.classShort;
		row.curHP = self.curHP; row.maxHP = self.maxHP;
		row.curMana = self.curMana; row.maxMana = self.maxMana;
		row.curEnd = self.curEnd; row.maxEnd = self.maxEnd;
		row.pctHP     = self.PctHP();
		row.pctMana   = self.PctMana();
		row.pctEnd    = self.PctEnd();
		row.manaClass = self.maxMana > 0;
		row.hasPet    = self.petId > 0;
		row.petPctHP  = self.petPctHP;
		row.petName   = self.petName;
		return row;
	}

	const myui::PeerRecord* peer = m_ctx.Actors ? m_ctx.Actors->FindPeerByName(snap.name) : nullptr;
	if (peer && peer->hasVitals)
	{
		row.hasData   = true;
		row.realValues = true;
		row.server    = peer->server;
		row.zoneName  = peer->zoneName;
		if (row.classShort.empty())
		{
			row.classShort = peer->classShort;
		}
		row.curHP = peer->curHP; row.maxHP = peer->maxHP;
		row.curMana = peer->curMana; row.maxMana = peer->maxMana;
		row.curEnd = peer->curEnd; row.maxEnd = peer->maxEnd;
		row.pctHP     = peer->PctHP();
		row.pctMana   = peer->PctMana();
		row.pctEnd    = peer->PctEnd();
		row.manaClass = peer->maxMana > 0;
		row.hasPet    = peer->petId > 0;
		row.petPctHP  = peer->petPctHP;
		row.petName   = peer->petName;
		row.standState = peer->standState;
		row.velocity  = peer->velocity;
		row.outOfZone = peer->zoneId != self.zoneId;
	}
	else if (snap.present)
	{
		row.hasData    = true;
		row.hasManaEnd = (snap.groupNumber == 0);
		row.pctHP      = snap.pctHP;
		row.pctMana    = snap.pctMana;
		row.pctEnd     = snap.pctEnd;
		row.manaClass  = snap.classId > 0
			&& snap.classId < static_cast<int>(sizeof(ClassInfo) / sizeof(ClassInfo[0]))
			&& ClassInfo[snap.classId].CanCast;
		row.hasPet     = snap.petId > 0;
		row.petPctHP   = snap.petPctHP;
	}

	return row;
}

void GroupModule::DrawMemberContextMenu(const GroupRowData& row)
{
	if (!ImGui::BeginPopupContextItem("##GroupMemberCtx"))
	{
		return;
	}

	ActorManager* ac = m_ctx.Actors;
	int navDist = static_cast<int>(m_ctx.UI->Num("Group", "NavDist", 10.0f));

	if (ImGui::Selectable("Switch To") && ac)
	{
		ac->SendCommand(row.server, row.name, "Switch");
	}

	if (ImGui::Selectable("Come To Me") && ac)
	{
		ac->SendCommand(row.server, row.name, "ComeTo", navDist);
	}

	if (ImGui::Selectable("Go To"))
	{
		if (row.spawnId > 0)
		{
			EzCommand(fmt::format("/nav id {} dist={} lineofsight=on", row.spawnId, navDist).c_str());
		}
		else
		{
			EzCommand(fmt::format("/nav spawn =\"{}\" dist={} lineofsight=on", row.name, navDist).c_str());
		}
	}

	if (ImGui::BeginMenu("Roles"))
	{
		if (ImGui::Selectable("Set Main Tank"))
		{
			EzCommand(fmt::format("/grouproles set {} 1", row.name).c_str());
		}
		if (ImGui::Selectable("Set Main Assist"))
		{
			EzCommand(fmt::format("/grouproles set {} 2", row.name).c_str());
		}
		if (ImGui::Selectable("Set Puller"))
		{
			EzCommand(fmt::format("/grouproles set {} 3", row.name).c_str());
		}
		ImGui::EndMenu();
	}

	if (ImGui::Selectable("Make Leader") && ac)
	{
		ac->SendCommand(row.server, row.name, "MakeLeader");
	}

	if (!row.isSelf)
	{
		ImGui::Separator();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.4f, 0.4f, 1.0f));
		bool kick = ImGui::Selectable("Kick");
		ImGui::PopStyleColor();
		if (kick)
		{
			myui::KickFromGroup(row.spawnId, row.name);
		}
	}

	ImGui::EndPopup();
}

void GroupModule::DrawMemberRow(const GroupRowData& row)
{
	const std::string& win = m_curWindow;
	float barH = m_ctx.UI->Bar(win, "HP").height;
	bool showLevel = m_ctx.UI->Flag("Group", "ShowLevel", true);
	bool showRoleIcons = m_ctx.UI->Flag("Group", "ShowRoleIcons", true);
	bool showMana = m_ctx.UI->Flag("Group", "ShowMana", true);
	bool showEnd = m_ctx.UI->Flag("Group", "ShowEnd", true);
	bool showPet = m_ctx.UI->Flag("Group", "ShowPet", true);
	bool vertPet = m_ctx.UI->Flag("Group", "VertPet", true);
	bool showMoveStatus = m_ctx.UI->Flag("Group", "ShowMoveStatus", true);
	bool showDistance = m_ctx.UI->Flag("Group", "ShowDistance", true);

	bool inZone = row.hasData && !row.outOfZone;
	bool sitting = inZone && row.standState == STANDSTATE_SIT;
	bool moving = inZone && row.velocity != 0.0f;
	bool showMove = showMoveStatus && inZone;
	bool showDist = showDistance && inZone && row.distance > 0.0f;

	ImGui::PushID(row.name.c_str());

	if (ImGui::BeginTable("##gmhdr", showLevel ? 3 : 2,
		ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
	{
		ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("icons", ImGuiTableColumnFlags_WidthFixed, barH * 5.0f + 12.0f);
		if (showLevel)
		{
			ImGui::TableSetupColumn("lvl", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		}

		ImGui::TableNextColumn();
		if (row.isLeader)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
		}
		bool clicked = ImGui::Selectable(row.name.c_str(), false);
		if (row.isLeader)
		{
			ImGui::PopStyleColor();
		}

		if (clicked)
		{
			TargetMember(row);
		}
		DrawMemberContextMenu(row);
		if (!showLevel && ImGui::IsItemHovered())
		{
			DrawMemberTooltip(row);
		}
		if (showDist)
		{
			ImGui::SameLine();
			ImVec4 distColor = row.distance > 200.0f ? ImVec4(0.9f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
			ImGui::TextColored(distColor, "%d", static_cast<int>(row.distance));
		}

		ImGui::TableNextColumn();
		{
			int iconSz = static_cast<int>(barH);
			bool showEye = row.present;
			bool showRoles = showRoleIcons;

			int roleCount = showRoles ? ((row.isLeader ? 1 : 0)
				+ ((row.roleMask & myui::kRoleMainTank) ? 1 : 0)
				+ ((row.roleMask & myui::kRoleMainAssist) ? 1 : 0)
				+ ((row.roleMask & myui::kRolePuller) ? 1 : 0)) : 0;
			int n = (showMove ? 1 : 0) + (showEye ? 1 : 0) + roleCount;

			if (n > 0)
			{
				float totalW = n * static_cast<float>(iconSz) + (n - 1) * 2.0f;
				float cellW = ImGui::GetContentRegionAvail().x;
				if (cellW > totalW)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cellW - totalW) * 0.5f);
				}

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, ImGui::GetStyle().ItemSpacing.y));
				bool first = true;
				auto glyph = [&](const char* g, const ImVec4& col, const char* tip)
				{
					if (!first)
					{
						ImGui::SameLine();
					}
					first = false;
					ImGui::TextColored(col, "%s", g);
					if (tip && ImGui::IsItemHovered())
					{
						ImGui::SetItemTooltip("%s", tip);
					}
				};
				auto anim = [&](const char* a, const char* fb, const ImVec4& col, const char* tip)
				{
					if (!first)
					{
						ImGui::SameLine();
					}
					first = false;
					if (m_ctx.Icons && m_ctx.Icons->DrawStatusIcon(a, iconSz, tip))
					{
						return;
					}
					ImGui::TextColored(col, "%s", fb);
					if (tip && ImGui::IsItemHovered())
					{
						ImGui::SetItemTooltip("%s", tip);
					}
				};

				if (showMove)
				{
					if (sitting)
					{
						glyph(ICON_FA_MOON_O, ImVec4(0.95f, 0.6f, 0.2f, 1.0f), "Sitting");
					}
					else if (moving)
					{
						glyph(ICON_MD_DIRECTIONS_RUN, ImVec4(0.25f, 0.85f, 0.80f, 1.0f), "Moving");
					}
					else
					{
						glyph(ICON_FA_SMILE_O, ImVec4(0.4f, 0.85f, 0.4f, 1.0f), "Standing");
					}
				}
				if (showEye)
				{
					if (row.inLOS)
					{
						glyph(ICON_FA_EYE, ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "In Line of Sight");
					}
					else
					{
						glyph(ICON_FA_EYE_SLASH, ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "No Line of Sight");
					}
				}
				if (showRoles)
				{
					if (row.isLeader)
					{
						glyph(ICON_FA_STAR, ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Group Leader");
					}
					if (row.roleMask & myui::kRoleMainTank)
					{
						anim("A_Tank", "[MT]", ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Main Tank");
					}
					if (row.roleMask & myui::kRoleMainAssist)
					{
						anim("A_Assist", "[MA]", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Main Assist");
					}
					if (row.roleMask & myui::kRolePuller)
					{
						anim("A_Puller", "[PL]", ImVec4(0.7f, 0.9f, 0.4f, 1.0f), "Puller");
					}
				}
				ImGui::PopStyleVar();
			}
		}

		if (showLevel)
		{
			ImGui::TableNextColumn();
			if (sitting)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.6f, 0.2f, 1.0f));
			}
			ImGui::Text("%d", row.level);
			if (sitting)
			{
				ImGui::PopStyleColor();
			}
			if (ImGui::IsItemHovered())
			{
				DrawMemberTooltip(row);
			}
		}

		ImGui::EndTable();
	}

	if (!row.hasData)
	{
		ImGui::TextDisabled(row.outOfZone ? "  (out of zone)" : "  (no data)");
		ImGui::PopID();
		return;
	}

	if (vertPet && showPet && row.hasPet)
	{
		BarStyle petBar = m_ctx.UI->Bar(win, "Pet");
		petBar.vertical = true;
		int bars = 1 + ((showMana && row.manaClass && row.hasManaEnd) ? 1 : 0) + ((showEnd && row.hasManaEnd) ? 1 : 0);
		float h = barH * bars + ImGui::GetStyle().ItemSpacing.y * (bars - 1);
		petBar.height = h;
		ApplyOutOfZoneColor(petBar, row.outOfZone);

		if (ImGui::BeginTable("##rowbars", 2, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("bars", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("pet", ImGuiTableColumnFlags_WidthFixed, petBar.width > 0.0f ? petBar.width : 16.0f);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			DrawVitalBars(row);
			ImGui::TableNextColumn();
			myui::DrawStyledBar("##pet", static_cast<float>(row.petPctHP), petBar);
			if (ImGui::IsItemClicked() && row.petId > 0)
			{
				myui::GiveItemTo(row.petId);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetItemTooltip("%s: %d%%", row.petName.empty() ? "Pet" : row.petName.c_str(), row.petPctHP);
			}
			ImGui::EndTable();
		}
	}
	else
	{
		DrawVitalBars(row);
		if (showPet && row.hasPet)
		{
			BarStyle petBar = m_ctx.UI->Bar(win, "Pet");
			petBar.vertical = false;
			ApplyOutOfZoneColor(petBar, row.outOfZone);
			myui::DrawStyledBar("##pet", static_cast<float>(row.petPctHP), petBar);
			if (ImGui::IsItemClicked() && row.petId > 0)
			{
				myui::GiveItemTo(row.petId);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetItemTooltip("%s: %d%%", row.petName.empty() ? "Pet" : row.petName.c_str(), row.petPctHP);
			}
		}
	}

	ImGui::PopID();
}

void GroupModule::TargetMember(const GroupRowData& row)
{
	if (row.spawnId > 0)
	{
		myui::GiveItemTo(row.spawnId);
	}
	else
	{
		EzCommand(fmt::format("/target {}", row.name).c_str());
	}
}

void GroupModule::ApplyOutOfZoneColor(BarStyle& bar, bool outOfZone) const
{
	if (!outOfZone)
	{
		return;
	}
	MQColor c = m_ctx.UI->Color("Group", "OutOfZoneColor", MQColor(116, 116, 116, 255));
	bar.fillLow = c;
	bar.fillHigh = c;
}

void GroupModule::DrawVitalBars(const GroupRowData& row)
{
	const std::string& win = m_curWindow;
	bool showMana = m_ctx.UI->Flag("Group", "ShowMana", true);
	bool showEnd = m_ctx.UI->Flag("Group", "ShowEnd", true);

	BarStyle hpBar = m_ctx.UI->Bar(win, "HP");
	ApplyOutOfZoneColor(hpBar, row.outOfZone);

	int hpCur = row.realValues ? row.curHP : -1;
	int hpMax = row.realValues ? row.maxHP : -1;
	myui::DrawStyledBar("##hp", static_cast<float>(row.pctHP), hpBar, hpCur, hpMax);
	if (ImGui::IsItemClicked())
	{
		TargetMember(row);
	}

	if (showMana && row.manaClass && row.hasManaEnd)
	{
		BarStyle manaBar = m_ctx.UI->Bar(win, "Mana");
		ApplyOutOfZoneColor(manaBar, row.outOfZone);
		int cur = row.realValues ? row.curMana : -1;
		int max = row.realValues ? row.maxMana : -1;
		myui::DrawStyledBar("##mana", static_cast<float>(row.pctMana), manaBar, cur, max);
		if (ImGui::IsItemClicked())
		{
			TargetMember(row);
		}
	}

	if (showEnd && row.hasManaEnd)
	{
		BarStyle endBar = m_ctx.UI->Bar(win, "End");
		ApplyOutOfZoneColor(endBar, row.outOfZone);
		int cur = row.realValues ? row.curEnd : -1;
		int max = row.realValues ? row.maxEnd : -1;
		myui::DrawStyledBar("##end", static_cast<float>(row.pctEnd), endBar, cur, max);
		if (ImGui::IsItemClicked())
		{
			TargetMember(row);
		}
	}
}

void GroupModule::DrawMemberTooltip(const GroupRowData& row)
{
	ImGui::BeginTooltip();
	ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s (%d)", row.name.c_str(), row.level);
	if (!row.classShort.empty())
	{
		ImGui::Text("Class: %s", row.classShort.c_str());
	}

	if (row.realValues)
	{
		ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.6f, 1.0f), "Health: %d / %d (%d%%)", row.curHP, row.maxHP, row.pctHP);
		if (row.manaClass)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.95f, 1.0f), "Mana: %d / %d (%d%%)", row.curMana, row.maxMana, row.pctMana);
		}
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.1f, 1.0f), "End: %d / %d (%d%%)", row.curEnd, row.maxEnd, row.pctEnd);
	}
	else
	{
		ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.6f, 1.0f), "Health: %d%%", row.pctHP);
		if (row.hasManaEnd && row.manaClass)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.95f, 1.0f), "Mana: %d%%", row.pctMana);
		}
		if (row.hasManaEnd)
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.1f, 1.0f), "End: %d%%", row.pctEnd);
		}
	}

	if (!row.outOfZone && row.distance > 0.0f)
	{
		ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Distance: %.1f", row.distance);
	}
	if (!row.zoneName.empty())
	{
		ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Zone: %s", row.zoneName.c_str());
	}
	if (row.hasPet)
	{
		ImGui::Text("Pet: %s (%d%% health)", row.petName.empty() ? "pet" : row.petName.c_str(), row.petPctHP);
	}

	if (row.roleMask & myui::kRoleMainTank)
	{
		ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Main Tank");
	}
	if (row.roleMask & myui::kRoleMainAssist)
	{
		ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Main Assist");
	}
	if (row.roleMask & myui::kRolePuller)
	{
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.4f, 1.0f), "Puller");
	}

	ImGui::EndTooltip();
}

void GroupModule::SendToMembers(const std::vector<GroupMemberSnap>& members, const std::string& action, int arg, const std::string& argStr)
{
	ActorManager* ac = m_ctx.Actors;
	if (!ac)
	{
		return;
	}

	for (const GroupMemberSnap& m : members)
	{
		if (m.isSelf)
		{
			continue;
		}
		ac->SendCommand("", m.name, action, arg, argStr);
	}
}

void GroupModule::DrawCommandButtons(const std::vector<GroupMemberSnap>& members, const char* scopeLabel, bool& followActive)
{
	if (!m_ctx.Actors || !pLocalPlayer)
	{
		return;
	}

	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float halfW = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

	if (myui::StyledButton("Come to Me", ImVec2(halfW, 0.0f)))
	{
		int zoneId = pZoneInfo ? static_cast<int>(pZoneInfo->ZoneID) : 0;
		std::string loc = fmt::format("{:.2f} {:.2f} {:.2f}", pLocalPlayer->X, pLocalPlayer->Y, pLocalPlayer->Z);
		SendToMembers(members, "ComeLoc", zoneId, loc);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("Send a /nav to your exact location to every %s member running MyUI (only those in your zone respond).", scopeLabel);
	}

	ImGui::SameLine();

	bool active = followActive;
	if (active)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.35f, 0.60f, 1.0f));
	}
	if (myui::StyledButton(active ? "Following" : "Follow Me", ImVec2(halfW, 0.0f)))
	{
		SendToMembers(members, active ? "FollowStop" : "FollowMe");
		followActive = !active;
	}
	if (active)
	{
		ImGui::PopStyleColor(1);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("Toggle every %s member running MyUI to /afollow you (only those in your zone start following). Resets on zone.", scopeLabel);
	}
}

void GroupModule::OnZone()
{
	m_groupFollow = false;
	m_raidFollow = false;
}

void GroupModule::DrawGroupWindow()
{
	CharData* ch = m_ctx.Char;
	const auto& members = ch->Group();

	bool showSelf = m_ctx.UI->Flag("Group", "ShowSelf", false);

	int shown = 0;
	bool hasOthers = false;
	for (const GroupMemberSnap& snap : members)
	{
		if (!snap.isSelf)
		{
			hasOthers = true;
		}

		if (snap.isSelf && !showSelf)
		{
			continue;
		}

		GroupRowData row = BuildRow(snap);
		DrawMemberRow(row);
		ImGui::Separator();
		++shown;
	}

	if (m_ctx.UI->Flag("Group", "ShowEmptySlots", false) && !members.empty())
	{
		float barH = m_ctx.UI->Bar("Group", "HP").height;
		int emptySlots = MAX_GROUP_SIZE - static_cast<int>(members.size());
		for (int i = 0; i < emptySlots; ++i)
		{
			ImGui::PushID(2000 + i);
			ImGui::TextDisabled("(empty)");
			ImGui::Dummy(ImVec2(0.0f, barH));
			ImGui::Separator();
			ImGui::PopID();
		}
	}

	if (shown == 0)
	{
		ImGui::TextDisabled("Not in a group");
	}

	if (hasOthers)
	{
		DrawCommandButtons(members, "group", m_groupFollow);
	}
}

void GroupModule::DrawRaidDummyTile()
{
	float barH = m_ctx.UI->Bar("Raid", "HP").height;
	ImGui::TextDisabled("(empty)");
	ImGui::Dummy(ImVec2(0.0f, barH));
}

void GroupModule::DrawRaidGroupBlock(int groupNumber, const std::vector<const GroupMemberSnap*>& members)
{
	ImGui::PushID(groupNumber);
	ImGui::TextDisabled("Group %d", groupNumber);
	ImGui::Separator();

	for (int slot = 0; slot < 6; ++slot)
	{
		if (slot < static_cast<int>(members.size()))
		{
			GroupRowData row = BuildRow(*members[slot]);
			DrawMemberRow(row);
		}
		else
		{
			ImGui::PushID(1000 + slot);
			DrawRaidDummyTile();
			ImGui::PopID();
		}
		ImGui::Separator();
	}

	ImGui::PopID();
}

void GroupModule::DrawRaidWindow()
{
	CharData* ch = m_ctx.Char;
	const auto& members = ch->Raid();
	if (members.empty())
	{
		ImGui::TextDisabled("Not in a raid");
		return;
	}

	std::map<int, std::vector<const GroupMemberSnap*>> groups;
	for (const GroupMemberSnap& snap : members)
	{
		groups[snap.groupNumber].push_back(&snap);
	}

	float tileWidth = m_ctx.UI->Num("Raid", "TileWidth", 180.0f);
	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float avail = ImGui::GetContentRegionAvail().x;
	int groupsPerRow = static_cast<int>(avail / (tileWidth + spacing));
	if (groupsPerRow < 1)
	{
		groupsPerRow = 1;
	}

	if (ImGui::BeginTable("##RaidGroups", groupsPerRow, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV))
	{
		for (int c = 0; c < groupsPerRow; ++c)
		{
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, tileWidth);
		}

		for (const auto& [groupNumber, list] : groups)
		{
			ImGui::TableNextColumn();
			DrawRaidGroupBlock(groupNumber, list);
		}
		ImGui::EndTable();
	}

	ImGui::Separator();
	DrawCommandButtons(members, "raid", m_raidFollow);
}

void GroupModule::OnRenderGUI()
{
	if (!m_ctx.Char || !m_ctx.Char->IsIngame())
	{
		return;
	}

	WindowConfig& gw = m_ctx.UI->Window("Group");
	if (gw.visible)
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
		if (gw.locked)
		{
			flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		}

		ImGui::SetNextWindowSize(ImVec2(216, 0), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("MyUI Group##MyUIGroup", &gw.visible, flags))
		{
			ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * gw.textScale);

			m_curWindow = "Group";
			DrawGroupWindow();

			ImGui::PopFont();
		}
		ImGui::End();

		if (!gw.visible)
		{
			m_ctx.UI->PersistVisibility("Group");
		}
	}

	WindowConfig& rw = m_ctx.UI->Window("Raid");
	if (rw.visible && m_ctx.Char->InRaid())
	{
		ImGuiWindowFlags rflags = ImGuiWindowFlags_NoScrollbar;
		if (rw.locked)
		{
			rflags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		}

		ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("MyUI Raid##MyUIRaid", &rw.visible, rflags))
		{
			ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * rw.textScale);

			m_curWindow = "Raid";
			DrawRaidWindow();

			ImGui::PopFont();
		}
		ImGui::End();

		if (!rw.visible)
		{
			m_ctx.UI->PersistVisibility("Raid");
		}
	}
}
