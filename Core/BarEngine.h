#pragma once

#include <mq/Plugin.h>

struct BarStyle;

namespace myui
{
void DrawStyledBar(const char* id, float pct, const BarStyle& style, int curValue = -1, int maxValue = -1);

void DrawStyledBarRect(const char* id, float pct, const BarStyle& style, const ImVec2& p0, const ImVec2& p1,
	int curValue = -1, int maxValue = -1);

// Draws `text` at `pos` with its glyph coverage used as an alpha mask, filling the
// glyphs with a vertical colorA->colorB gradient that scrolls downward and cycles
// over time. Reserves no layout space (advance the cursor yourself).
void DrawScrollingGradientText(ImDrawList* dl, const ImVec2& pos, const char* text,
	const ImVec4& colorA, const ImVec4& colorB);
}
