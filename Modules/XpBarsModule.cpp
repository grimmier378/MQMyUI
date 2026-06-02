#include "XpBarsModule.h"

#include "../Core/UiConfig.h"
#include "../Core/ThemeManager.h"
#include "../Core/ActorManager.h"
#include "../Core/PeerData.h"
#include "../Core/CharData.h"
#include "../Core/BarEngine.h"

#include "imgui/fonts/IconsFontAwesome.h"

#include <algorithm>
#include <vector>

struct AARow
{
	std::string name;
	std::string server;
	std::string groupLeader;
	std::string allocation;
	int level = 0;
	int aaAvail = 0;
	int aaSpent = 0;
	int aaTotal = 0;
	float pctAAExp = 0.0f;
	float pctExp = 0.0f;
	float pctAir = 100.0f;
	bool isSelf = false;
};

namespace
{
bool InGroup(CharData* ch, const std::string& name)
{
	for (const GroupMemberSnap& m : ch->Group())
	{
		if (ci_equals(m.name, name))
		{
			return true;
		}
	}
	return false;
}
} // namespace

void XpBarsModule::DrawRow(const AARow& row)
{
	float scale = m_ctx.UI->Window(GetName()).textScale;
	bool showTooltip = m_ctx.UI->Flag(GetName(), "ShowTooltip", true);

	bool& expand = m_expand[row.name];
	bool& compact = m_compact[row.name];

	float tileW = 165.0f * scale;

	ImGui::PushID(row.name.c_str());
	ImGui::BeginChild("##tile", ImVec2(tileW, 0.0f),
		ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar);

	ImGui::BeginGroup();

	if (ImGui::BeginTable("##aatbl", 2, ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("pts", ImGuiTableColumnFlags_WidthFixed, 38.0f * scale);

		ImGui::TableNextColumn();
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s", row.name.c_str());
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%d", row.level);

		ImGui::TableNextColumn();
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%d", row.aaAvail);

		ImGui::EndTable();
	}

	if (!compact)
	{
		myui::DrawStyledBar("##xp", row.pctExp, m_ctx.UI->Bar(GetName(), "XP"));
		myui::DrawStyledBar("##aaxp", row.pctAAExp, m_ctx.UI->Bar(GetName(), "AAExp"));
		if (row.pctAir < 100.0f)
		{
			myui::DrawStyledBar("##air", row.pctAir, m_ctx.UI->Bar(GetName(), "Air"));
		}
	}

	ImGui::EndGroup();

	bool tileHovered = ImGui::IsItemHovered();
	if (tileHovered)
	{
		if (ImGui::GetIO().KeyCtrl && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_ctx.Actors)
		{
			m_ctx.Actors->SendCommand(row.server, row.name, "Switch");
		}
		else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			expand = !expand;
		}
		else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
		{
			compact = !compact;
		}
	}

	if (showTooltip && tileHovered)
	{
		ImGui::BeginTooltip();
		ImGui::Text("%s (Level %d)", row.name.c_str(), row.level);
		ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Exp:    %.2f%%", row.pctExp);
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "AA Exp: %.2f%%", row.pctAAExp);
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Avail:  %d", row.aaAvail);
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Spent:  %d", row.aaSpent);
		ImGui::TextColored(ImVec4(0.7f, 0.5f, 1.0f, 1.0f), "Total:  %d", row.aaTotal);
		ImGui::EndTooltip();
	}

	if (expand && m_ctx.Actors)
	{
		ActorManager* ac = m_ctx.Actors;

		std::string alloc = row.allocation;
		size_t pctPos = alloc.find('%');
		if (pctPos != std::string::npos)
		{
			alloc.erase(pctPos);
		}
		if (alloc.empty())
		{
			alloc = "0";
		}

		if (ImGui::SmallButton(ICON_FA_ANGLE_DOUBLE_LEFT))
		{
			ac->SendCommand(row.server, row.name, "Min");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(ICON_FA_ANGLE_LEFT))
		{
			ac->SendCommand(row.server, row.name, "Less");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(fmt::format("{}%", alloc).c_str()))
		{
			ac->SendCommand(row.server, row.name, "Mid");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(ICON_FA_ANGLE_RIGHT))
		{
			ac->SendCommand(row.server, row.name, "More");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(ICON_FA_ANGLE_DOUBLE_RIGHT))
		{
			ac->SendCommand(row.server, row.name, "Max");
		}
	}

	ImGui::EndChild();
	ImGui::PopID();
}

void XpBarsModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame())
	{
		return;
	}

	bool autoSize = m_ctx.UI->Flag(GetName(), "AutoSize", false);
	bool alphaSort = m_ctx.UI->Flag(GetName(), "AlphaSort", false);
	bool myGroupOnly = m_ctx.UI->Flag(GetName(), "MyGroupOnly", true);
	bool showLeader = m_ctx.UI->Flag(GetName(), "ShowLeader", false);

	CharData* ch = m_ctx.Char;
	const CharSnapshot& self = ch->Get();

	std::vector<AARow> rows;

	{
		AARow r;
		r.name        = self.name;
		r.server      = self.server;
		r.groupLeader = self.groupLeader;
		r.level       = self.level;
		r.aaAvail     = self.aaAvailable;
		r.aaSpent     = self.aaSpent;
		r.aaTotal     = self.aaTotal;
		r.pctAAExp    = self.pctAAExp;
		r.pctExp      = self.pctExp;
		r.pctAir      = self.pctAir;
		r.allocation  = self.allocation;
		r.isSelf      = true;
		rows.push_back(std::move(r));
	}

	if (m_ctx.Actors)
	{
		for (const auto& [key, peer] : m_ctx.Actors->Peers())
		{
			if (!peer.hasAA)
			{
				continue;
			}
			if (myGroupOnly && !InGroup(ch, peer.character))
			{
				continue;
			}

			AARow r;
			r.name        = peer.character;
			r.server      = peer.server;
			r.groupLeader = peer.groupLeader;
			r.level       = peer.level;
			r.aaAvail     = peer.aaAvailable;
			r.aaSpent     = peer.aaSpent;
			r.aaTotal     = peer.aaTotal;
			r.pctAAExp    = peer.pctAAExp;
			r.pctExp      = peer.pctExp;
			r.pctAir      = peer.pctAir;
			r.allocation  = peer.allocation;
			rows.push_back(std::move(r));
		}
	}

	if (alphaSort)
	{
		std::sort(rows.begin(), rows.end(), [](const AARow& a, const AARow& b) {
			if (ci_equals(a.groupLeader, b.groupLeader))
			{
				return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
			}
			return _stricmp(a.groupLeader.c_str(), b.groupLeader.c_str()) < 0;
		});
	}
	else
	{
		std::stable_sort(rows.begin(), rows.end(), [](const AARow& a, const AARow& b) {
			return _stricmp(a.groupLeader.c_str(), b.groupLeader.c_str()) < 0;
		});
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}
	if (autoSize)
	{
		flags |= ImGuiWindowFlags_AlwaysAutoResize;
	}

	ImGui::SetNextWindowSize(ImVec2(185, 480), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("XP Bars##MyUIXPBars", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		float avail = ImGui::GetContentRegionAvail().x;
		float tileW = 165.0f * w.textScale;
		float spacing = ImGui::GetStyle().ItemSpacing.x;
		float rowUsed = 0.0f;
		std::string lastLeader;
		bool first = true;
		for (const AARow& row : rows)
		{
			if (showLeader && (first || !ci_equals(row.groupLeader, lastLeader)))
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Leader: %s",
					row.groupLeader.empty() ? "NoGroup" : row.groupLeader.c_str());
				ImGui::Separator();
				lastLeader = row.groupLeader;
				rowUsed = 0.0f;
			}
			first = false;

			if (rowUsed > 0.0f)
			{
				if (rowUsed + spacing + tileW <= avail)
				{
					ImGui::SameLine();
					rowUsed += spacing;
				}
				else
				{
					rowUsed = 0.0f;
				}
			}

			DrawRow(row);
			rowUsed += tileW;
		}

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}
}
