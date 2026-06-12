#pragma once

#include "CharData.h"
#include "UiHelpers.h"

#include <functional>
#include <vector>

class IconHelper;

namespace myui
{
struct BuffIconLineupOpts
{
	float pulse = 0.0f;
	int iconSize = 24;
	int flashThreshold = 18;
	bool trimTrailingEmpty = false;
	bool showCaster = true;
	bool withHours = false;
	MQColor selfCast = kBuffSelfCast;
	std::function<void(const BuffInfo&)> contextMenu;
};

void DrawBuffIconLineup(const char* idKey, const std::vector<BuffInfo>& buffs,
	const BuffIconLineupOpts& opts, IconHelper* icons);
} // namespace myui
