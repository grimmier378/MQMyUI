#include "IconHelper.h"

#include "mq/imgui/Widgets.h"

#include <cstdio>

CTextureAnimation* IconHelper::Get(CTextureAnimation*& slot, const char* animName)
{
	if (!slot && pSidlMgr)
	{
		if (CTextureAnimation* temp = pSidlMgr->FindAnimation(animName))
		{
			slot = new CTextureAnimation();
			*slot = *temp;
		}
	}
	return slot;
}

CTextureAnimation* IconHelper::SpellIcons()    { return Get(m_spellIcons, "A_SpellGems"); }
CTextureAnimation* IconHelper::GemBackground() { return Get(m_gemBackground, "A_SpellGemBackground"); }
CTextureAnimation* IconHelper::GemHolder()     { return Get(m_gemHolder, "A_SpellGemHolder"); }
CTextureAnimation* IconHelper::SpellBook()     { return Get(m_spellBook, "A_SpellBook"); }
CTextureAnimation* IconHelper::ItemIcon()      { return Get(m_itemIcon, "A_DragItem"); }

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

	CTextureAnimation* anim = nullptr;
	if (pSidlMgr)
	{
		if (CTextureAnimation* temp = pSidlMgr->FindAnimation(animName))
		{
			anim = new CTextureAnimation();
			*anim = *temp;
		}
	}
	m_named[animName] = anim;
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

void IconHelper::DrawEmptySlot(int slotNumber, const CXSize& size)
{
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImVec2 sz(static_cast<float>(size.cx), static_cast<float>(size.cy));
	ImGui::Dummy(sz);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), IM_COL32(110, 110, 110, 160));

	char num[16];
	sprintf_s(num, "%d", slotNumber);
	ImVec2 ts = ImGui::CalcTextSize(num);
	dl->AddText(ImVec2(p.x + (sz.x - ts.x) * 0.5f, p.y + (sz.y - ts.y) * 0.5f),
		IM_COL32(150, 150, 150, 200), num);
}

void IconHelper::Shutdown()
{
	delete m_spellIcons;    m_spellIcons = nullptr;
	delete m_gemBackground; m_gemBackground = nullptr;
	delete m_gemHolder;     m_gemHolder = nullptr;
	delete m_spellBook;     m_spellBook = nullptr;
	delete m_itemIcon;      m_itemIcon = nullptr;

	for (auto& [name, anim] : m_named)
		delete anim;
	m_named.clear();
}
