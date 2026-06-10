#include "IconHelper.h"

#include "mq/imgui/Widgets.h"

#include <cstdio>

CTextureAnimation* IconHelper::SpellIcons()    { return Named("A_SpellGems"); }
CTextureAnimation* IconHelper::GemBackground() { return Named("A_SpellGemBackground"); }
CTextureAnimation* IconHelper::GemHolder()     { return Named("A_SpellGemHolder"); }
CTextureAnimation* IconHelper::SpellBook()     { return Named("A_SpellBook"); }
CTextureAnimation* IconHelper::ItemIcon()      { return Named("A_DragItem"); }

void IconHelper::DrawSpellIcon(int iconId, const CXSize& size, const MQColor& tint, const MQColor& border)
{
	CTextureAnimation* anim = SpellIcons();
	if (!anim)
	{
		ImGui::Dummy(ImVec2(static_cast<float>(size.cx), static_cast<float>(size.cy)));
		return;
	}

	anim->SetCurCell(iconId);
	imgui::DrawTextureAnimation(anim, size, tint, border);
}

CTextureAnimation* IconHelper::Named(const char* animName)
{
	auto it = m_named.find(animName);
	if (it != m_named.end())
	{
		return it->second;
	}

	// Only cache a successful load so a not-yet-ready pSidlMgr/FindAnimation is
	// retried on the next call (matching the original per-slot lazy loaders).
	CTextureAnimation* anim = nullptr;
	if (pSidlMgr)
	{
		if (CTextureAnimation* temp = pSidlMgr->FindAnimation(animName))
		{
			anim = new CTextureAnimation();
			*anim = *temp;
			m_named[animName] = anim;
		}
	}
	return anim;
}

bool IconHelper::DrawStatusIcon(const char* animName, int sizePx, const char* tooltip)
{
	CTextureAnimation* anim = Named(animName);
	if (!anim)
	{
		return false;
	}

	imgui::DrawTextureAnimation(anim, CXSize(sizePx, sizePx), MQColor(255, 255, 255, 255), MQColor(0, 0, 0, 0));
	if (tooltip && ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("%s", tooltip);
	}
	return true;
}

void IconHelper::DrawEmptySlot(const CXSize& size)
{
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImVec2 sz(static_cast<float>(size.cx), static_cast<float>(size.cy));
	ImGui::Dummy(sz);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), IM_COL32(110, 110, 110, 160));
}

void IconHelper::Shutdown()
{
	for (auto& [name, anim] : m_named)
	{
		delete anim;
	}
	m_named.clear();
}
