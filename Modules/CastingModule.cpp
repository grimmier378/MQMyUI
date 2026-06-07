#include "CastingModule.h"

#include "../Core/UiConfig.h"
#include "../Core/BarEngine.h"
#include "../Core/ThemeManager.h"

#include <cassert>

void CastingModule::OnPulse()
{
	bool castingNow = pLocalPlayer && pDisplay && pLocalPlayer->CastingData.SpellID != -1
		&& pLocalPlayer->CastingData.SpellETA != 0;

	if (castingNow && !m_casting)
	{
		m_castEta = pLocalPlayer->CastingData.SpellETA;
		float total = static_cast<float>(
			static_cast<int64_t>(m_castEta) - static_cast<int64_t>(pDisplay->TimeStamp));
		m_castTotalMs = total > 0.0f ? total : 0.0f;

		m_spellName.clear();
		if (EQ_Spell* spell = GetSpellByID(pLocalPlayer->CastingData.SpellID))
		{
			m_spellName = spell->Name;
		}

		m_casting = true;
	}
	else if (!castingNow)
	{
		m_casting = false;
	}
}

void CastingModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	const bool preview = m_ctx.UI->previewCasting;

	float progress = 0.6f;
	float remainSec = 0.0f;

	if (!preview)
	{
		if (!w.visible || !m_casting || m_castTotalMs <= 0.0f || !pDisplay)
		{
			return;
		}

		float remainMs = static_cast<float>(
			static_cast<int64_t>(m_castEta) - static_cast<int64_t>(pDisplay->TimeStamp));
		if (remainMs < 0.0f)
		{
			remainMs = 0.0f;
		}
		else if (remainMs > m_castTotalMs)
		{
			remainMs = m_castTotalMs;
		}

		progress = remainMs / m_castTotalMs;
		remainSec = remainMs / 1000.0f;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoFocusOnAppearing;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Casting##MyUICasting", nullptr, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		myui::DrawStyledBar("##cast", progress * 100.0f, m_ctx.UI->Bar(GetName(), "Cast"));

		if (preview)
		{
			ImGui::TextUnformatted("Casting Preview");
		}
		else
		{
			ImGui::Text("%s  %0.1fs", m_spellName.c_str(), remainSec);
		}

		ImGui::PopFont();
	}
	ImGui::End();
}
