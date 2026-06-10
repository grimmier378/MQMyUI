#pragma once

#include <mq/Plugin.h>

#include <string>
#include <unordered_map>

class IconHelper
{
public:
	void Shutdown();

	CTextureAnimation* SpellIcons();
	CTextureAnimation* GemBackground();
	CTextureAnimation* GemHolder();
	CTextureAnimation* SpellBook();
	CTextureAnimation* ItemIcon();

	void DrawSpellIcon(int iconId, const CXSize& size, const MQColor& tint, const MQColor& border);

	void DrawEmptySlot(const CXSize& size);

	bool DrawStatusIcon(const char* animName, int sizePx, const char* tooltip = nullptr);

private:
	CTextureAnimation* Named(const char* animName);

	std::unordered_map<std::string, CTextureAnimation*> m_named;
};
