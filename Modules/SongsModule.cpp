#include "SongsModule.h"

#include "../Core/UiConfig.h"
#include "../Core/ThemeManager.h"
#include "../Core/CharData.h"
#include "../Core/IconHelper.h"
#include "../Core/ColorUtil.h"

#include <cmath>
#include <cstdio>

namespace
{
const MQColor kBorder(80, 120, 255, 255);
const MQColor kBorderDet(250, 0, 0, 255);

void FormatDuration(char* out, size_t size, int ms)
{
	if (ms < 0)
	{
		strcpy_s(out, size, "perm");
		return;
	}
	int totalSec = ms / 1000;
	int minutes = totalSec / 60;
	int seconds = totalSec % 60;
	if (minutes > 0)
	{
		sprintf_s(out, size, "%dm %ds", minutes, seconds);
	}
	else
	{
		sprintf_s(out, size, "%ds", seconds);
	}
}
} // namespace

void SongsModule::OnPulse()
{
	if (m_ctx.UI && m_ctx.UI->Window(GetName()).visible && m_ctx.Char)
	{
		m_ctx.Char->RefreshSongs();
	}
}

void SongsModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !m_ctx.Icons)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	ImGui::SetNextWindowSize(ImVec2(230, 220), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Songs##MyUISongs", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f);

		if (m_ctx.UI->Flag(GetName(), "IconView", false))
		{
			DrawIconView(pulse);
		}
		else
		{
			DrawTableView(pulse);
		}

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}
}

static void SongContextMenu(const BuffInfo& song)
{
	if (ImGui::BeginPopupContextItem("##SongCtx"))
	{
		if (ImGui::MenuItem("Remove"))
		{
			DoCommandf("/removebuff \"%s\"", song.name.c_str());
		}
		ImGui::EndPopup();
	}
}

void SongsModule::DrawTableView(float pulse)
{
	float iconSize = 22.0f * m_ctx.UI->Window(GetName()).iconScale;
	bool showTimer = m_ctx.UI->Flag(GetName(), "ShowTimer", true);
	int flashThreshold = static_cast<int>(m_ctx.UI->Num(GetName(), "FlashThreshold", 18.0f));

	if (!ImGui::BeginTable("##SongsTable", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
	{
		return;
	}

	ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, iconSize + 4);
	ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (const BuffInfo& song : m_ctx.Char->Songs())
	{
		if (song.isEmpty)
		{
			continue;
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::PushID(song.slot);

		MQColor border = song.beneficial ? kBorder : kBorderDet;
		int secondsLeft = song.durationMs / 1000;
		MQColor tint = myui::FlashTint(secondsLeft, flashThreshold, pulse);

		m_ctx.Icons->DrawSpellIcon(song.iconId, CXSize(static_cast<int>(iconSize), static_cast<int>(iconSize)), tint, border);
		SongContextMenu(song);

		ImGui::TableNextColumn();
		if (showTimer && song.durationMs != 0)
		{
			char timeLabel[32];
			FormatDuration(timeLabel, sizeof(timeLabel), song.durationMs);
			ImGui::TextColored(MQColor(255, 165, 0, 255).ToImColor(), "%s", timeLabel);
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(song.name.c_str());

		ImGui::PopID();
	}

	ImGui::EndTable();
}

void SongsModule::DrawIconView(float pulse)
{
	if (!ImGui::BeginTable("##SongIcons", 2,
		ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
	{
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGui::TableSetupColumn("Songs", ImGuiTableColumnFlags_WidthStretch);

	const char* name = (pLocalPC && !mq::IsAnonymized()) ? pLocalPC->Name : "Me";
	DrawIconLineupRow(name, m_ctx.Char->Songs(), pulse);

	ImGui::EndTable();
}

void SongsModule::DrawIconLineupRow(const char* rowName, const std::vector<BuffInfo>& songs, float pulse)
{
	int iconSize = static_cast<int>(22.0f * m_ctx.UI->Window(GetName()).iconScale);
	int flashThreshold = static_cast<int>(m_ctx.UI->Num(GetName(), "FlashThreshold", 18.0f));

	int last = -1;
	for (int k = 0; k < static_cast<int>(songs.size()); ++k)
	{
		if (!songs[k].isEmpty)
		{
			last = k;
		}
	}

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextUnformatted(rowName);

	ImGui::TableSetColumnIndex(1);
	ImGui::PushID(rowName);

	float avail = ImGui::GetContentRegionAvail().x;
	float lineWidth = 0.0f;

	for (int k = 0; k <= last; ++k)
	{
		const BuffInfo& song = songs[k];
		ImGui::PushID(song.slot);

		if (song.isEmpty)
		{
			m_ctx.Icons->DrawEmptySlot(song.slot + 1, CXSize(iconSize, iconSize));
		}
		else
		{
			MQColor border = song.beneficial ? kBorder : kBorderDet;
			int secondsLeft = song.durationMs / 1000;
			MQColor tint = myui::FlashTint(secondsLeft, flashThreshold, pulse);

			m_ctx.Icons->DrawSpellIcon(song.iconId, CXSize(iconSize, iconSize), tint, border);

			if (ImGui::IsItemHovered())
			{
				char timeLabel[32];
				FormatDuration(timeLabel, sizeof(timeLabel), song.durationMs);
				ImGui::BeginTooltip();
				ImGui::Text("%s", song.name.c_str());
				if (song.durationMs != 0)
				{
					ImGui::Text("Time: %s", timeLabel);
				}
				ImGui::EndTooltip();
			}

			SongContextMenu(song);
		}

		ImGui::PopID();

		lineWidth += iconSize + 2.0f;
		if (lineWidth < avail - iconSize)
		{
			ImGui::SameLine(0.0f, 2.0f);
		}
		else
		{
			lineWidth = 0.0f;
		}
	}

	ImGui::PopID();
}
