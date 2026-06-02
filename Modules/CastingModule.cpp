#include "CastingModule.h"

#include "../Core/UiConfig.h"
#include "../Core/BarEngine.h"
#include "../Core/ThemeManager.h"

void CastingModule::OnPulse()
{
	bool visible = pCastingWnd && pCastingWnd->IsVisible();

	if (visible && !m_wasVisible)
	{
		const char* spellName = "";
		if (CXWnd* nameWnd = pCastingWnd->GetChildItem("Casting_SpellName"))
		{
			spellName = nameWnd->WindowText.c_str();
		}

		if (EQ_Spell* spell = GetSpellByName(spellName))
		{
			m_casting = true;
			m_spellName = spellName;
			m_castTotalMs = static_cast<float>(spell->CastTime + GetCastingTimeModifier(spell));
			m_castStart = std::chrono::steady_clock::now();
		}
	}
	else if (!visible)
	{
		m_casting = false;
	}

	m_wasVisible = visible;
}

void CastingModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	const bool preview = m_ctx.UI->previewCasting;

	float progress = 0.6f;
	float remainSec = 0.0f;

	if (!preview)
	{
		if (!w.visible || !m_casting)
		{
			return;
		}

		auto now = std::chrono::steady_clock::now();
		float elapsedMs = static_cast<float>(
			std::chrono::duration_cast<std::chrono::milliseconds>(now - m_castStart).count());

		if (m_castTotalMs <= 0.0f || elapsedMs >= m_castTotalMs)
		{
			m_casting = false;
			return;
		}

		progress = 1.0f - (elapsedMs / m_castTotalMs);
		remainSec = (m_castTotalMs - elapsedMs) / 1000.0f;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove;
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
