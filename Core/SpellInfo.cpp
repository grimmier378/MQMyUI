#include "SpellInfo.h"

#include <mq/Plugin.h>

namespace myui
{
std::string SpellEffectText(int spellId)
{
	if (spellId <= 0)
	{
		return "";
	}
	EQ_Spell* spell = GetSpellByID(spellId);
	if (!spell)
	{
		return "";
	}
	char buf[2048] = { 0 };
	ShowSpellSlotInfo(spell, buf, sizeof(buf), "\n");
	return buf;
}

void RenderSpellEffects(int spellId)
{
	std::string eff = SpellEffectText(spellId);
	if (eff.empty())
	{
		return;
	}
	ImGui::Separator();
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
	ImGui::TextColored(ImVec4(0.47f, 0.67f, 0.90f, 1.0f), "%s", eff.c_str());
	ImGui::PopTextWrapPos();
}
} // namespace myui
