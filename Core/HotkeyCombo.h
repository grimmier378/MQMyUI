#pragma once

#include <string>

struct WindowConfig;

namespace myui
{
struct ToggleCombo
{
	std::string key;
	std::string mod1;
	std::string mod2;
	std::string mod3;
	std::string mouse;
};

ToggleCombo ReadCombo(const WindowConfig& w);
bool CheckToggleCombo(const ToggleCombo& combo);
} // namespace myui
