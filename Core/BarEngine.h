#pragma once

#include <mq/Plugin.h>

struct BarStyle;

namespace myui
{
void DrawStyledBar(const char* id, float pct, const BarStyle& style, int curValue = -1, int maxValue = -1);
}
