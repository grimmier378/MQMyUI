#include "PlayerModule.h"

#include "../Core/UiConfig.h"
#include "../Core/BarEngine.h"
#include "../Core/ThemeManager.h"
#include "../Core/CharData.h"
#include "../Core/IconHelper.h"
#include "../Core/UiHelpers.h"
#include "../Core/PeerData.h"
#include "../Core/Widgets.h"

#include "imgui/fonts/IconsFontAwesome.h"

#include <cmath>
#include <cstdio>

namespace
{
const MQColor kBuffBeneficial(80, 120, 255, 255);
const MQColor kBuffDetrimental(250, 0, 0, 255);
const MQColor kBuffSelfCast(190, 190, 20, 255);
const MQColor kBuffTint(255, 255, 255, 255);

const char* StateAnimFor(const std::string& s)
{
	if (ci_equals(s, "COMBAT"))
	{
		return "A_PWCSInCombat";
	}
	if (ci_equals(s, "RESTING"))
	{
		return "A_PWCSRegen";
	}
	if (ci_equals(s, "COOLDOWN"))
	{
		return "A_PWCSTimer";
	}
	if (ci_equals(s, "DEBUFFED"))
	{
		return "A_PWCSDebuff";
	}
	return "A_PWCSStanding";
}

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

void PlayerModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !pLocalPlayer)
	{
		return;
	}

	bool showTarget = m_ctx.UI->Flag(GetName(), "ShowTarget", true);
	bool splitTarget = m_ctx.UI->Flag(GetName(), "SplitTarget", false);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	{
		ImGui::SetNextWindowSize(ImVec2(230, 0), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("MyUI Player##MyUIPlayer", &w.visible, flags))
		{
			ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

			DrawPlayer();

			if (showTarget && !splitTarget)
			{
				ImGui::Separator();
				DrawTarget();
			}

			ImGui::PopFont();
		}
		ImGui::End();

		if (!w.visible)
		{
			m_ctx.UI->PersistVisibility(GetName());
		}
	}

	if (showTarget && splitTarget)
	{
		ImGui::SetNextWindowSize(ImVec2(230, 0), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("MyUI Target##MyUITarget", nullptr, flags))
		{
			ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

			DrawTarget();

			ImGui::PopFont();
		}
		ImGui::End();
	}
}

void PlayerModule::DrawPlayer()
{
	const CharSnapshot& c = m_ctx.Char->Get();
	const char* name = mq::IsAnonymized() ? "Me" : c.name.c_str();

	float barH = m_ctx.UI->Bar(GetName(), "HP").height;
	bool combatPulse = m_ctx.UI->Flag(GetName(), "CombatPulse", true);

	bool combat = c.inCombat && combatPulse;
	if (combat)
	{
		float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9f, 0.1f, 0.1f, pulse));
	}

	ImGui::BeginChild("##playerblk", ImVec2(0.0f, 0.0f),
		ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar);

	int isz = static_cast<int>(barH);

	if (ImGui::BeginTable("##pinfo", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
	{
		ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("icons", ImGuiTableColumnFlags_WidthFixed, barH * 4.0f + 8.0f);
		ImGui::TableSetupColumn("lvl", ImGuiTableColumnFlags_WidthFixed, 0.0f);

		ImGui::TableNextColumn();

		if (c.inCombat)
		{
			ImVec2 namePos = ImGui::GetCursorScreenPos();
			myui::DrawScrollingGradientText(ImGui::GetWindowDrawList(), namePos, name,
				ImVec4(0.9f, 0.1f, 0.1f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ImGui::Dummy(ImGui::CalcTextSize(name));
		}
		else
		{
			ImGui::Text( "%s", name);
		}

		ImGui::TableNextColumn();
		if (m_ctx.Icons)
		{
			int n = 1
				+ ((c.roleMask & myui::kRoleMainTank) ? 1 : 0)
				+ ((c.roleMask & myui::kRoleMainAssist) ? 1 : 0)
				+ ((c.roleMask & myui::kRolePuller) ? 1 : 0);
			float totalW = n * static_cast<float>(isz) + (n - 1) * 2.0f;
			float cellW = ImGui::GetContentRegionAvail().x;
			if (cellW > totalW)
			{
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cellW - totalW) * 0.5f);
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, ImGui::GetStyle().ItemSpacing.y));
			m_ctx.Icons->DrawStatusIcon(StateAnimFor(c.combatState), isz, c.combatState.c_str());
			if (c.roleMask & myui::kRoleMainTank)   { ImGui::SameLine(); m_ctx.Icons->DrawStatusIcon("A_Tank", isz, "Main Tank"); }
			if (c.roleMask & myui::kRoleMainAssist) { ImGui::SameLine(); m_ctx.Icons->DrawStatusIcon("A_Assist", isz, "Main Assist"); }
			if (c.roleMask & myui::kRolePuller)     { ImGui::SameLine(); m_ctx.Icons->DrawStatusIcon("A_Puller", isz, "Puller"); }
			ImGui::PopStyleVar();
		}

		ImGui::TableNextColumn();
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%d", c.level);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s (%d)", name, c.level);
			if (!c.classShort.empty())
			{
				ImGui::Text("Class: %s", c.classShort.c_str());
			}
			ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.6f, 1.0f), "Health: %d / %d (%d%%)", c.curHP, c.maxHP, c.PctHP());
			if (c.maxMana > 0)
			{
				ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.95f, 1.0f), "Mana: %d / %d (%d%%)", c.curMana, c.maxMana, c.PctMana());
			}
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.1f, 1.0f), "End: %d / %d (%d%%)", c.curEnd, c.maxEnd, c.PctEnd());
			if (!c.zoneName.empty())
			{
				ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Zone: %s", c.zoneName.c_str());
			}
			ImGui::EndTooltip();
		}

		ImGui::EndTable();
	}

	float hpPct = c.maxHP > 0 ? (100.0f * c.curHP / c.maxHP) : 0.0f;
	myui::DrawStyledBar("##hp", hpPct, m_ctx.UI->Bar(GetName(), "HP"), c.curHP, c.maxHP);

	if (c.isCaster)
	{
		float manaPct = c.maxMana > 0 ? (100.0f * c.curMana / c.maxMana) : 0.0f;
		myui::DrawStyledBar("##mana", manaPct, m_ctx.UI->Bar(GetName(), "Mana"), c.curMana, c.maxMana);
	}

	float endPct = c.maxEnd > 0 ? (100.0f * c.curEnd / c.maxEnd) : 0.0f;
	myui::DrawStyledBar("##end", endPct, m_ctx.UI->Bar(GetName(), "End"), c.curEnd, c.maxEnd);

	ImGui::EndChild();

	if (combat)
	{
		ImGui::PopStyleColor();
	}
}

void PlayerModule::DrawTarget()
{
	if (!pTarget || !pLocalPlayer)
	{
		ImGui::TextDisabled("No Target");
		return;
	}

	uint32_t argb = ConColorToARGB(ConColor(pTarget));
	ImVec4 conCol = MQColor(MQColor::format_argb, argb).ToImColor();

	ImGui::PushStyleColor(ImGuiCol_Border, conCol);
	ImGui::BeginChild("##tgtblk", ImVec2(0.0f, 0.0f),
		ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar);

	std::string targetName;
	if (mq::IsAnonymized() && pTarget->Type == SPAWN_PLAYER)
	{
		const char* tcls = pTarget->GetClassThreeLetterCode();
		targetName = myui::MaskedCode(pTarget->GetRace(), tcls ? tcls : "", pTarget->GetLevel());
	}
	else
	{
		targetName = myui::TrimName(pTarget->DisplayedName);
	}

	int pct = static_cast<int>(pTarget->HPCurrent);
	bool overlayText = m_ctx.UI->Flag(GetName(), "TargetTextOverlay", false);

	if (overlayText)
	{
		const BarStyle& tbar = m_ctx.UI->Bar(GetName(), "TargetHP");
		float barH = tbar.height > 1.0f ? tbar.height : 1.0f;
		float textH = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 4.0f;
		float regionH = barH > textH ? barH : textH;
		float width = ImGui::GetContentRegionAvail().x;

		ImVec2 top = ImGui::GetCursorScreenPos();
		ImVec2 barP0(top.x, top.y + (regionH - barH));
		ImVec2 barP1(top.x + width, top.y + regionH);
		myui::DrawStyledBarRect("##thp", static_cast<float>(pct), tbar, barP0, barP1);

		DrawTargetOverlayText(top, ImVec2(top.x + width, top.y + regionH), targetName);

		ImGui::Dummy(ImVec2(width, regionH));
	}
	else
	{
		DrawTargetInfo(targetName);
		myui::DrawStyledBar("##thp", static_cast<float>(pct), m_ctx.UI->Bar(GetName(), "TargetHP"));
	}

	if (pTarget->Type == SPAWN_NPC)
	{
		int aggro = m_ctx.Char->Get().pctAggro;
		myui::DrawStyledBar("##taggro", static_cast<float>(aggro), m_ctx.UI->Bar(GetName(), "Aggro"));
	}

	DrawTargetBuffs();

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void PlayerModule::DrawTargetInfo(const std::string& targetName)
{
	bool los = pLocalPlayer->CanSee(*pTarget);
	char distbuf[24];
	sprintf_s(distbuf, "%.0f", GetDistance(pLocalPlayer, pTarget));
	const char* eye = los ? ICON_FA_EYE : ICON_FA_EYE_SLASH;

	ImGui::Text("%s", targetName.c_str());

	RingStyle& ring = m_ctx.UI->Ring(GetName(), "Direction");
	bool showRing = m_ctx.UI->Flag(GetName(), "ShowDirectionRing", false) && pTarget != pLocalPlayer;
	float distW = ImGui::CalcTextSize(distbuf).x;
	float rightBlockW = distW;

	ImGui::SameLine();
	float startX = ImGui::GetCursorPosX();
	float avail = ImGui::GetContentRegionAvail().x;
	float rightStartX = startX + avail - rightBlockW;

	// Visibility indicator centered between the name and the right-aligned distance.
	float eyeW = ImGui::CalcTextSize(eye).x;
	float eyeX = startX + ((rightStartX - startX) - eyeW) * 0.5f;
	if (eyeX < startX)
	{
		eyeX = startX;
	}
	ImGui::SetCursorPosX(eyeX);
	ImGui::TextColored(los ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
		"%s", eye);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("%s", los ? "In Line of Sight" : "No Line of Sight");
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(rightStartX);
	if (showRing)
	{
		// Reserve only the distance-text footprint; draw the ring + text on the
		// window's own draw list (covered by windows placed on top) with the clip
		// widened to full-screen so it escapes the cell. No reflow on ring resize.
		ImVec2 p = ImGui::GetCursorScreenPos();
		ImVec2 center(p.x + distW * 0.5f, p.y + ImGui::GetTextLineHeight() * 0.5f);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->PushClipRectFullScreen();
		myui::DrawDirectionRing(dl, ImGui::GetID("##tgtring"),
			center, myui::RelativeBearingDeg(pTarget->X, pTarget->Y),
			GetDistance(pLocalPlayer, pTarget), los, distbuf, ring);
		dl->PopClipRect();
		ImGui::Dummy(ImVec2(distW, ImGui::GetTextLineHeight()));
	}
	else
	{
		ImGui::Text("%s", distbuf);
	}

	const char* tcls = pTarget->GetClassThreeLetterCode();
	const char* tbody = GetBodyTypeDesc(GetBodyType(pTarget));
	bool showBody = tbody && tbody[0] && tbody[0] != '*';
	if (showBody)
	{
		ImGui::TextDisabled("Lvl %d %s %s", pTarget->Level, tcls ? tcls : "", tbody);
	}
	else
	{
		ImGui::TextDisabled("Lvl %d %s", pTarget->Level, tcls ? tcls : "");
	}
}

void PlayerModule::DrawTargetOverlayText(const ImVec2& rmin, const ImVec2& rmax, const std::string& targetName)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 shadow = IM_COL32(0, 0, 0, 180);
	auto drawText = [&](float x, float y, ImU32 col, const char* s)
	{
		dl->AddText(ImVec2(x + 1.0f, y + 1.0f), shadow, s);
		dl->AddText(ImVec2(x, y), col, s);
	};

	float padX = ImGui::GetStyle().FramePadding.x + 2.0f;
	float x = rmin.x + padX;
	float y = rmin.y + 2.0f;
	float lineH = ImGui::GetTextLineHeightWithSpacing();

	drawText(x, y, IM_COL32(255, 255, 255, 255), targetName.c_str());

	bool los = pLocalPlayer->CanSee(*pTarget);
	const char* eye = los ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
	char distbuf[24];
	sprintf_s(distbuf, "%.0f", GetDistance(pLocalPlayer, pTarget));

	float distW = ImGui::CalcTextSize(distbuf).x;
	float eyeW = ImGui::CalcTextSize(eye).x;
	float gap = ImGui::GetStyle().ItemSpacing.x;
	float distX = rmax.x - padX - distW;

	RingStyle& ring = m_ctx.UI->Ring(GetName(), "Direction");
	if (m_ctx.UI->Flag(GetName(), "ShowDirectionRing", false) && pTarget != pLocalPlayer)
	{
		ImVec2 center(distX + distW * 0.5f, y + ImGui::GetTextLineHeight() * 0.5f);
		dl->PushClipRectFullScreen();
		myui::DrawDirectionRing(dl, ImGui::GetID("##tgtringovl"), center,
			myui::RelativeBearingDeg(pTarget->X, pTarget->Y),
			GetDistance(pLocalPlayer, pTarget), los, distbuf, ring);
		dl->PopClipRect();
	}
	else
	{
		drawText(distX, y, IM_COL32(255, 255, 255, 255), distbuf);
	}
	drawText(distX - gap - eyeW, y, los ? IM_COL32(80, 230, 80, 255) : IM_COL32(230, 80, 80, 255), eye);

	const char* tcls = pTarget->GetClassThreeLetterCode();
	const char* tbody = GetBodyTypeDesc(GetBodyType(pTarget));
	bool showBody = tbody && tbody[0] && tbody[0] != '*';
	char lvlbuf[96];
	if (showBody)
	{
		sprintf_s(lvlbuf, "Lvl %d %s %s", pTarget->Level, tcls ? tcls : "", tbody);
	}
	else
	{
		sprintf_s(lvlbuf, "Lvl %d %s", pTarget->Level, tcls ? tcls : "");
	}
	drawText(x, y + lineH, IM_COL32(170, 170, 170, 255), lvlbuf);
}

void PlayerModule::DrawTargetBuffs()
{
	if (!m_ctx.Icons || !pTargetWnd)
	{
		return;
	}

	WindowConfig& w = m_ctx.UI->Window(GetName());
	int iconSize = static_cast<int>(20.0f * w.iconScale);
	float avail = ImGui::GetContentRegionAvail().x;
	float lineWidth = 0.0f;

	for (const auto& buffInfo : pTargetWnd->GetBuffRange())
	{
		EQ_Spell* spell = buffInfo.GetSpell();
		if (!spell)
		{
			continue;
		}

		ImGui::PushID(spell->ID);

		const char* caster = buffInfo.GetCaster();
		MQColor border = spell->IsBeneficialSpell() ? kBuffBeneficial : kBuffDetrimental;
		if (!spell->IsBeneficialSpell() && pLocalPC && caster && ci_equals(caster, pLocalPC->Name))
		{
			border = kBuffSelfCast;
		}
		m_ctx.Icons->DrawSpellIcon(spell->SpellIcon, CXSize(iconSize, iconSize), kBuffTint, border);

		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && pSpellDisplayManager)
		{
			pSpellDisplayManager->ShowSpell(spell->ID, true, true, SpellDisplayType_SpellBookWnd);
		}

		if (ImGui::IsItemHovered())
		{
			char timeLabel[32];
			FormatDuration(timeLabel, sizeof(timeLabel), buffInfo.GetBuffTimer());
			ImGui::BeginTooltip();
			ImGui::Text("%s", spell->Name);
			ImGui::Text("Time: %s", timeLabel);
			ImGui::EndTooltip();
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
}
