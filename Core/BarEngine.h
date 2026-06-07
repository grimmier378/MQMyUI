#pragma once

#include <mq/Plugin.h>

struct BarStyle;

namespace myui
{
void DrawStyledBar(const char* id, float pct, const BarStyle& style, int curValue = -1, int maxValue = -1);

void DrawStyledBarRect(const char* id, float pct, const BarStyle& style, const ImVec2& p0, const ImVec2& p1,
	int curValue = -1, int maxValue = -1);
}
