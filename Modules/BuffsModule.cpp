#include "BuffsModule.h"

#include "../Core/UiConfig.h"
#include "../Core/ThemeManager.h"
#include "../Core/CharData.h"
#include "../Core/IconHelper.h"
#include "../Core/ActorManager.h"
#include "../Core/PeerData.h"
#include "../Core/ColorUtil.h"
#include "../Core/UiHelpers.h"
#include "../Core/Widgets.h"

#include <cmath>
#include <cstdio>

void BuffsModule::OnPulse()
{
	if (m_ctx.UI && m_ctx.UI->Window(GetName()).visible && m_ctx.Char)
	{
		m_ctx.Char->RefreshBuffs();
	}
}

void BuffsModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !m_ctx.Icons || !pLocalPC)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	ImGui::SetNextWindowSize(ImVec2(240, 320), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Buffs##MyUIBuffs", &w.visible, flags))
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

void BuffsModule::BuffContextMenu(const BuffInfo& buff, const std::string& targetChar)
{
	if (ImGui::BeginPopupContextItem("##BuffCtx"))
	{
		bool isSelf = targetChar.empty();
		if (ImGui::MenuItem("Remove"))
		{
			if (isSelf)
			{
				DoCommandf("/removebuff \"%s\"", buff.name.c_str());
			}
			else if (m_ctx.Actors)
			{
				m_ctx.Actors->SendCommand("", targetChar, "RemoveBuff", 0, buff.name);
			}
		}
		if (ImGui::MenuItem("Block"))
		{
			if (isSelf)
			{
				DoCommandf("/blockspell add me %d", buff.spellId);
			}
			else if (m_ctx.Actors)
			{
				m_ctx.Actors->SendCommand("", targetChar, "BlockBuff", buff.spellId);
			}
		}
		if (ImGui::MenuItem("Inspect"))
		{
			if (pSpellDisplayManager)
			{
				pSpellDisplayManager->ShowSpell(buff.spellId, true, true, SpellDisplayType_SpellBookWnd);
			}
		}
		ImGui::EndPopup();
	}
}

void BuffsModule::DrawTableView(float pulse)
{
	float iconSize = 24.0f * m_ctx.UI->Window(GetName()).iconScale;
	int flashThreshold = static_cast<int>(m_ctx.UI->Num(GetName(), "FlashThreshold", 18.0f));
	bool groupOnly = m_ctx.UI->Flag(GetName(), "ShowGroupOnly", true);

	const char* selfName = (pLocalPC && !mq::IsAnonymized()) ? pLocalPC->Name : "Me";

	std::string current;
	if (m_selectedChar.empty())
	{
		current = selfName;
	}
	else if (!mq::IsAnonymized())
	{
		current = m_selectedChar;
	}
	else
	{
		current = "Player"; // anonymized preview of the selected character
		if (m_ctx.Char)
		{
			for (const GroupMemberSnap& gm : m_ctx.Char->Group())
			{
				if (ci_equals(gm.name, m_selectedChar)) { current = gm.maskedName; break; }
			}
		}
		if (current == "Player" && m_ctx.Actors)
		{
			if (const myui::PeerRecord* pr = m_ctx.Actors->FindPeerByName(m_selectedChar)) { current = pr->maskedName; }
		}
	}
	ImGui::SetNextItemWidth(-1.0f);
	if (myui::StyledBeginCombo("##BuffChar", current.c_str()))
	{
		if (myui::PillSelectable(selfName, m_selectedChar.empty()))
		{
			m_selectedChar.clear();
			ImGui::CloseCurrentPopup();
		}
		if (groupOnly && m_ctx.Char)
		{
			for (const GroupMemberSnap& gm : m_ctx.Char->Group())
			{
				if (gm.isSelf)
				{
					continue;
				}
				std::string disp = (mq::IsAnonymized() ? gm.maskedName : gm.name) + "##" + gm.name;
				if (myui::PillSelectable(disp.c_str(), ci_equals(m_selectedChar, gm.name)))
				{
					m_selectedChar = gm.name;
					ImGui::CloseCurrentPopup();
				}
			}
		}
		else if (!groupOnly && m_ctx.Actors)
		{
			for (const auto& [key, peer] : m_ctx.Actors->Peers())
			{
				if (ci_equals(peer.character, selfName))
				{
					continue;
				}
				std::string disp = (mq::IsAnonymized() ? peer.maskedName : peer.character) + "##" + peer.character;
				if (myui::PillSelectable(disp.c_str(), ci_equals(m_selectedChar, peer.character)))
				{
					m_selectedChar = peer.character;
					ImGui::CloseCurrentPopup();
				}
			}
		}
		myui::StyledEndCombo();
	}

	static const std::vector<BuffInfo> kNoBuffs;
	bool isSelf = m_selectedChar.empty() || ci_equals(m_selectedChar, selfName);
	const std::vector<BuffInfo>* buffList = &m_ctx.Char->Buffs();
	if (!isSelf)
	{
		const myui::PeerRecord* peer = m_ctx.Actors ? m_ctx.Actors->FindPeerByName(m_selectedChar) : nullptr;
		buffList = (peer && peer->hasBuffs) ? &peer->buffs : &kNoBuffs;
	}

	if (!ImGui::BeginTable("##BuffsTable", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Hideable))
	{
		return;
	}

	ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, iconSize + 4);
	ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (const BuffInfo& buff : *buffList)
	{
		ImGui::TableNextRow();
		ImGui::PushID(buff.slot);

		if (buff.isEmpty)
		{
			if (ImGui::TableSetColumnIndex(0))
			{
				m_ctx.Icons->DrawEmptySlot(CXSize(static_cast<int>(iconSize), static_cast<int>(iconSize)));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetItemTooltip("Slot %d (Empty)", buff.slot + 1);
				}
			}
			ImGui::PopID();
			continue;
		}

		MQColor border = myui::BuffBorderColor(buff.beneficial, buff.caster.c_str());

		int secondsLeft = buff.durationMs / 1000;
		MQColor tint = myui::FlashTint(secondsLeft, flashThreshold, pulse);

		if (ImGui::TableSetColumnIndex(0))
		{
			m_ctx.Icons->DrawSpellIcon(buff.iconId, CXSize(static_cast<int>(iconSize), static_cast<int>(iconSize)), tint, border);
			BuffContextMenu(buff, isSelf ? std::string() : m_selectedChar);
		}

		if (ImGui::TableSetColumnIndex(1) && buff.durationMs != 0)
		{
			char timeLabel[32];
			myui::FormatDuration(timeLabel, sizeof(timeLabel), buff.durationMs, true);
			ImGui::TextColored(MQColor(255, 165, 0, 255).ToImColor(), "%s", timeLabel);
		}

		if (ImGui::TableSetColumnIndex(2))
		{
			ImGui::TextUnformatted(buff.name.c_str());
		}

		ImGui::PopID();
	}

	ImGui::EndTable();
}

void BuffsModule::DrawIconView(float pulse)
{
	if (!ImGui::BeginTable("##BuffIcons", 2,
		ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
	{
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGui::TableSetupColumn("Buffs", ImGuiTableColumnFlags_WidthStretch);

	const char* name = (pLocalPC && !mq::IsAnonymized()) ? pLocalPC->Name : "Me";
	DrawIconLineupRow(name, m_ctx.Char->Buffs(), pulse, std::string());

	bool groupOnly = m_ctx.UI->Flag(GetName(), "ShowGroupOnly", true);

	if (m_ctx.Actors && m_ctx.Char)
	{
		static const std::vector<BuffInfo> kNoBuffs;
		if (groupOnly)
		{
			for (const GroupMemberSnap& gm : m_ctx.Char->Group())
			{
				if (gm.isSelf)
				{
					continue;
				}
				const myui::PeerRecord* peer = m_ctx.Actors->FindPeerByName(gm.name);
				DrawIconLineupRow((mq::IsAnonymized() ? gm.maskedName : gm.name).c_str(), (peer && peer->hasBuffs) ? peer->buffs : kNoBuffs, pulse, gm.name);
			}
		}
		else
		{
			for (const auto& [key, peer] : m_ctx.Actors->Peers())
			{
				if (ci_equals(peer.character, name))
				{
					continue;
				}
				DrawIconLineupRow((mq::IsAnonymized() ? peer.maskedName : peer.character).c_str(), peer.hasBuffs ? peer.buffs : kNoBuffs, pulse, peer.character);
			}
		}
	}

	ImGui::EndTable();
}

void BuffsModule::DrawIconLineupRow(const char* rowName, const std::vector<BuffInfo>& buffs, float pulse, const std::string& targetChar)
{
	int iconSize = static_cast<int>(24.0f * m_ctx.UI->Window(GetName()).iconScale);
	int flashThreshold = static_cast<int>(m_ctx.UI->Num(GetName(), "FlashThreshold", 18.0f));

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextUnformatted(rowName);

	ImGui::TableSetColumnIndex(1);
	ImGui::PushID(targetChar.empty() ? rowName : targetChar.c_str());

	float avail = ImGui::GetContentRegionAvail().x;
	float lineWidth = 0.0f;

	for (int k = 0; k < static_cast<int>(buffs.size()); ++k)
	{
		const BuffInfo& buff = buffs[k];
		ImGui::PushID(buff.slot);

		if (buff.isEmpty)
		{
			m_ctx.Icons->DrawEmptySlot(CXSize(iconSize, iconSize));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetItemTooltip("Slot %d (Empty)", buff.slot + 1);
			}
		}
		else
		{
			MQColor border = myui::BuffBorderColor(buff.beneficial, buff.caster.c_str());

			int secondsLeft = buff.durationMs / 1000;
			MQColor tint = myui::FlashTint(secondsLeft, flashThreshold, pulse);

			m_ctx.Icons->DrawSpellIcon(buff.iconId, CXSize(iconSize, iconSize), tint, border);

			if (ImGui::IsItemHovered())
			{
				char timeLabel[32];
				myui::FormatDuration(timeLabel, sizeof(timeLabel), buff.durationMs, true);
				ImGui::BeginTooltip();
				ImGui::Text("%s", buff.name.c_str());
				if (buff.durationMs != 0)
				{
					ImGui::Text("Time: %s", timeLabel);
				}
				if (!buff.caster.empty())
				{
					ImGui::Text("Caster: %s", mq::IsAnonymized() ? "Player" : buff.caster.c_str());
				}
				ImGui::EndTooltip();
			}

			BuffContextMenu(buff, targetChar);
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
