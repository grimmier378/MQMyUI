#include "HudModule.h"

#include "../Core/UiConfig.h"
#include "../Core/ThemeManager.h"
#include "../Core/CharData.h"

#include "mq/imgui/Widgets.h"
#include "main/MQ2SpellSearch.h"
#include "imgui/fonts/IconsFontAwesome.h"

#include <vector>

namespace
{
struct StatusFXData
{
	eEQSPA spaValue;
	int iconID;
	bool positiveFX;
	const char* tooltip;
};

const std::vector<StatusFXData> kStatusFX = {
	{ SPA_POISON,          42,  false, "Poisoned" },
	{ SPA_DISEASE,         41,  false, "Diseased" },
	{ SPA_BLINDNESS,       200, false, "Blind" },
	{ SPA_ROOT,            117, false, "Rooted" },
	{ SPA_CURSE,           159, false, "Cursed" },
	{ SPA_SLOW,            17,  false, "Slowed" },
	{ SPA_CORRUPTION,      160, false, "Corrupted" },
	{ SPA_SILENCE,         95,  false, "Silenced" },
	{ SPA_MOVEMENT_RATE,   5,   false, "Snared" },
	{ SPA_FEAR,            164, false, "Feared" },
	{ SPA_INVISIBILITY,    18,  true,  "Invisible" },
	{ SPA_INVIS_VS_UNDEAD, 33,  true,  "Invis vs Undead" },
	{ SPA_INVIS_VS_ANIMALS,34,  true,  "Invis vs Animals" },
};

const MQColor kBorderDetrimental(250, 0, 0, 255);
const MQColor kBorderBeneficial(80, 160, 255, 255);
const MQColor kTint(255, 255, 255, 255);
} // namespace

void HudModule::OnShutdown()
{
	if (m_statusIcon)
	{
		delete m_statusIcon;
		m_statusIcon = nullptr;
	}
}

void HudModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !pLocalPlayer || !pLocalPC)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f - 15.0f, vp->WorkPos.y), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI HUD##MyUIHud", &w.visible, flags))
	{
		if (ImGui::BeginPopupContextWindow("##HudCtx"))
		{
			if (ImGui::MenuItem("Open Settings"))
			{
				m_ctx.UI->Window("Settings").visible = true;
				m_ctx.UI->PersistVisibility("Settings");
			}
			bool locked = w.locked;
			if (ImGui::MenuItem("Locked", nullptr, &locked))
			{
				w.locked = locked;
				m_ctx.UI->PersistField(GetName(), "Locked");
			}
			bool showHp = m_ctx.UI->Flag(GetName(), "ShowHpMana", false);
			if (ImGui::MenuItem("Show HP/Mana", nullptr, &showHp))
			{
				m_ctx.UI->SetFlag(GetName(), "ShowHpMana", showHp);
			}
			ImGui::EndPopup();
		}

		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		if (m_ctx.UI->Flag(GetName(), "ShowHpMana", false))
		{
			const CharSnapshot& c = m_ctx.Char->Get();
			ImGui::Text("HP %d%%", c.PctHP());
			if (c.isCaster)
			{
				ImGui::SameLine();
				ImGui::Text("MP %d%%", c.PctMana());
			}
		}

		DrawStatusEffects();

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}
}

void HudModule::DrawStatusEffects()
{
	if (!pSidlMgr)
	{
		return;
	}

	if (!m_statusIcon)
	{
		if (CTextureAnimation* temp = pSidlMgr->FindAnimation("A_SpellIcons"))
		{
			m_statusIcon = new CTextureAnimation();
			*m_statusIcon = *temp;
		}
		else
		{
			return;
		}
	}

	float iconPx = 30.0f * m_ctx.UI->Window(GetName()).iconScale;
	bool any = false;
	CXSize iconSize(static_cast<int>(iconPx), static_cast<int>(iconPx));
	MQColor border = kBorderDetrimental;

	if (pLocalPlayer->StandState == STANDSTATE_SIT)
	{
		any = true;
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 2.0f);
		ImGui::TextColored(MQColor(255, 165, 0, 255).ToImColor(), ICON_FA_MOON_O);
		ImGui::PopFont();
		if (ImGui::IsItemClicked())
		{
			EzCommand("/stand");
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetItemTooltip("Sitting");
		}
		ImGui::SameLine();
	}

	for (const auto& fx : kStatusFX)
	{
		if (GetSelfBuff(SpellAffect(fx.spaValue, fx.positiveFX)) >= 0)
		{
			any = true;
			m_statusIcon->SetCurCell(fx.iconID);
			border = fx.positiveFX ? kBorderBeneficial : kBorderDetrimental;
			imgui::DrawTextureAnimation(m_statusIcon, iconSize, kTint, border);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetItemTooltip(fx.tooltip);
			}
			ImGui::SameLine();
		}
	}

	border = kBorderDetrimental;

	if (GetSelfBuff([](EQ_Spell* spell) {
			return SpellAffect(SPA_HP, false)(spell) && spell->IsDetrimentalSpell() && spell->IsDoTSpell();
		}) >= 0)
	{
		any = true;
		m_statusIcon->SetCurCell(140);
		imgui::DrawTextureAnimation(m_statusIcon, iconSize, kTint, border);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetItemTooltip("Damage over Time");
		}
		ImGui::SameLine();
	}

	if (GetSelfBuff([](EQ_Spell* spell) {
			return spell && (ci_find_substr(spell->Name, "Resurrection Sickness") != -1
				|| ci_find_substr(spell->Name, "Revival Sickness") != -1);
		}) >= 0)
	{
		any = true;
		m_statusIcon->SetCurCell(154);
		imgui::DrawTextureAnimation(m_statusIcon, iconSize, kTint, border);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetItemTooltip("Resurrection Sickness");
		}
		ImGui::SameLine();
	}

	if (pLocalPlayer->StandState == STANDSTATE_FEIGN)
	{
		any = true;
		m_statusIcon->SetCurCell(92);
		imgui::DrawTextureAnimation(m_statusIcon, iconSize, kTint, border);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetItemTooltip("Feign Death");
		}
		if (ImGui::IsItemClicked())
		{
			EzCommand("/stand");
		}
		ImGui::SameLine();
	}

	if (pLocalPC->Stunned)
	{
		any = true;
		m_statusIcon->SetCurCell(25);
		imgui::DrawTextureAnimation(m_statusIcon, iconSize, kTint, border);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetItemTooltip("Stunned");
		}
		ImGui::SameLine();
	}

	if (!any)
	{
		ImGui::Dummy(ImVec2(iconPx, iconPx));
	}
}
