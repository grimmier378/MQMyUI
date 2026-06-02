#include "HotkeyCombo.h"
#include "UiConfig.h"

#include <mq/Plugin.h>

#include <cctype>

namespace
{
std::string LookupStr(const WindowConfig& w, const std::string& name, const std::string& def)
{
	auto it = w.strs.find(name);
	return it != w.strs.end() ? it->second : def;
}

ImGuiKey KeyFromString(const std::string& s)
{
	if (s.empty())
	{
		return ImGuiKey_None;
	}

	if (s.size() == 1)
	{
		char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
		if (c >= 'A' && c <= 'Z')
		{
			return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'A'));
		}
		if (c >= '0' && c <= '9')
		{
			return static_cast<ImGuiKey>(ImGuiKey_0 + (c - '0'));
		}
		return ImGuiKey_None;
	}

	if (s[0] == 'F' || s[0] == 'f')
	{
		int n = mq::GetIntFromString(std::string_view(s).substr(1), 0);
		if (n >= 1 && n <= 12)
		{
			return static_cast<ImGuiKey>(ImGuiKey_F1 + (n - 1));
		}
	}

	return ImGuiKey_None;
}

bool ModifierDown(const std::string& mod)
{
	if (mod.empty() || ci_equals(mod, "None"))
	{
		return true;
	}

	ImGuiIO& io = ImGui::GetIO();
	if (ci_equals(mod, "Ctrl"))
	{
		return io.KeyCtrl;
	}
	if (ci_equals(mod, "Alt"))
	{
		return io.KeyAlt;
	}
	if (ci_equals(mod, "Shift"))
	{
		return io.KeyShift;
	}
	return true;
}
} // namespace

namespace myui
{
ToggleCombo ReadCombo(const WindowConfig& w)
{
	ToggleCombo combo;
	combo.key   = LookupStr(w, "ToggleKey", "");
	combo.mod1  = LookupStr(w, "ToggleMod1", "None");
	combo.mod2  = LookupStr(w, "ToggleMod2", "None");
	combo.mod3  = LookupStr(w, "ToggleMod3", "None");
	combo.mouse = LookupStr(w, "ToggleMouse", "None");
	return combo;
}

bool CheckToggleCombo(const ToggleCombo& combo)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput)
	{
		return false;
	}

	if (!ModifierDown(combo.mod1) || !ModifierDown(combo.mod2) || !ModifierDown(combo.mod3))
	{
		return false;
	}

	ImGuiKey key = KeyFromString(combo.key);
	if (key != ImGuiKey_None && ImGui::IsKeyPressed(key, false))
	{
		return true;
	}

	if (!ci_equals(combo.mouse, "None") && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
	{
		return true;
	}

	return false;
}
} // namespace myui
