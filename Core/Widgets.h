#pragma once

#include <mq/Plugin.h>

namespace myui
{
enum ToggleFlags
{
	ToggleFlags_None          = 0,
	ToggleFlags_StarKnob      = 1 << 0,
	ToggleFlags_RightLabel    = 1 << 1,
	ToggleFlags_AnimateKnob   = 1 << 2,
	ToggleFlags_GlowOnHover   = 1 << 3,
	ToggleFlags_AnimateOnHover = 1 << 4,
	ToggleFlags_PulseOnHover  = 1 << 5,
	ToggleFlags_KnobBorder    = 1 << 6,
};

constexpr int kDefaultToggleFlags =
	ToggleFlags_RightLabel | ToggleFlags_GlowOnHover | ToggleFlags_PulseOnHover;

constexpr int kUseGlobalToggleFlags = -1;

void SetGlobalToggleStyle(int flags, ImVec2 size = ImVec2(0.0f, 0.0f));

bool DrawToggle(const char* id, bool* value, int flags = kUseGlobalToggleFlags, ImVec2 size = ImVec2(0.0f, 0.0f));
}
