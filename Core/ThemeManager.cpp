#include "ThemeManager.h"
#include "SettingsStore.h"
#include "ColorUtil.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace
{
std::vector<float> ParseFloats(const std::string& s)
{
	std::vector<float> out;
	const char* p = s.c_str();
	const char* end = p + s.size();
	while (p < end)
	{
		if (*p == '-' || *p == '+' || *p == '.' || std::isdigit(static_cast<unsigned char>(*p)))
		{
			char* next = nullptr;
			float v = std::strtof(p, &next);
			if (next == p)
			{
				++p;
				continue;
			}
			out.push_back(v);
			p = next;
		}
		else
		{
			++p;
		}
	}
	return out;
}

int ColorIndexByName(const std::string& name)
{
	static const std::unordered_map<std::string, int> lookup = []
	{
		std::unordered_map<std::string, int> m;
		m.reserve(ImGuiCol_COUNT);
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			m.emplace(ImGui::GetStyleColorName(i), i);
		}
		return m;
	}();

	auto it = lookup.find(name);
	if (it != lookup.end())
	{
		return it->second;
	}
	return -1;
}

// Single source of truth for the scalar/vec2 style fields. These arrays drive
// ToRows/FromRows serialization, the shared-style defaults, the captured ImGui
// default, and ApplyTheme — adding a style field is one row here.
struct ScalarField
{
	const char*         name;
	float Theme::*      t;
	float ImGuiStyle::* s;
	float               def;
};

struct Vec2Field
{
	const char*          name;
	ImVec2 Theme::*      t;
	ImVec2 ImGuiStyle::* s;
	ImVec2               def;
};

const ScalarField kScalarFields[] = {
	{ "WindowRounding",    &Theme::windowRounding,    &ImGuiStyle::WindowRounding,    0.0f },
	{ "FrameRounding",     &Theme::frameRounding,     &ImGuiStyle::FrameRounding,     0.0f },
	{ "ChildRounding",     &Theme::childRounding,     &ImGuiStyle::ChildRounding,     0.0f },
	{ "PopupRounding",     &Theme::popupRounding,     &ImGuiStyle::PopupRounding,     0.0f },
	{ "ScrollbarRounding", &Theme::scrollbarRounding, &ImGuiStyle::ScrollbarRounding, 0.0f },
	{ "GrabRounding",      &Theme::grabRounding,      &ImGuiStyle::GrabRounding,      0.0f },
	{ "TabRounding",       &Theme::tabRounding,       &ImGuiStyle::TabRounding,       4.0f },
	{ "WindowBorderSize",  &Theme::windowBorderSize,  &ImGuiStyle::WindowBorderSize,  1.0f },
	{ "FrameBorderSize",   &Theme::frameBorderSize,   &ImGuiStyle::FrameBorderSize,   0.0f },
	{ "ChildBorderSize",   &Theme::childBorderSize,   &ImGuiStyle::ChildBorderSize,   1.0f },
	{ "PopupBorderSize",   &Theme::popupBorderSize,   &ImGuiStyle::PopupBorderSize,   1.0f },
	{ "TabBarBorderSize",  &Theme::tabBarBorderSize,  &ImGuiStyle::TabBarBorderSize,  1.0f },
	{ "IndentSpacing",     &Theme::indentSpacing,     &ImGuiStyle::IndentSpacing,     21.0f },
	{ "ScrollbarSize",     &Theme::scrollbarSize,     &ImGuiStyle::ScrollbarSize,     14.0f },
	{ "GrabMinSize",       &Theme::grabMinSize,       &ImGuiStyle::GrabMinSize,       12.0f },
};

const Vec2Field kVec2Fields[] = {
	{ "WindowPadding",    &Theme::windowPadding,    &ImGuiStyle::WindowPadding,    ImVec2(8.0f, 8.0f) },
	{ "FramePadding",     &Theme::framePadding,     &ImGuiStyle::FramePadding,     ImVec2(4.0f, 3.0f) },
	{ "CellPadding",      &Theme::cellPadding,      &ImGuiStyle::CellPadding,      ImVec2(4.0f, 2.0f) },
	{ "ItemSpacing",      &Theme::itemSpacing,      &ImGuiStyle::ItemSpacing,      ImVec2(8.0f, 4.0f) },
	{ "ItemInnerSpacing", &Theme::itemInnerSpacing, &ImGuiStyle::ItemInnerSpacing, ImVec2(4.0f, 4.0f) },
};

const ScalarField* FindScalarField(const std::string& name)
{
	for (const ScalarField& f : kScalarFields)
	{
		if (name == f.name)
		{
			return &f;
		}
	}
	return nullptr;
}

const Vec2Field* FindVec2Field(const std::string& name)
{
	for (const Vec2Field& f : kVec2Fields)
	{
		if (name == f.name)
		{
			return &f;
		}
	}
	return nullptr;
}

std::vector<ThemeRow> ToRows(const Theme& t)
{
	std::vector<ThemeRow> rows;
	rows.reserve(ImGuiCol_COUNT + 20);

	for (int i = 0; i < ImGuiCol_COUNT; ++i)
	{
		const ImVec4& c = t.colors[i];
		rows.push_back(ThemeRow{ ImGui::GetStyleColorName(i), "imvec4",
			fmt::format("{{{},{},{},{}}}", c.x, c.y, c.z, c.w) });
	}

	for (const ScalarField& f : kScalarFields)
	{
		rows.push_back(ThemeRow{ f.name, "float", fmt::format("{}", t.*f.t) });
	}
	for (const Vec2Field& f : kVec2Fields)
	{
		const ImVec2& v = t.*f.t;
		rows.push_back(ThemeRow{ f.name, "imvec2", fmt::format("{{{},{}}}", v.x, v.y) });
	}

	return rows;
}

Theme FromRows(const std::string& name, const std::vector<ThemeRow>& rows, const Theme& base)
{
	Theme t = base;
	t.name = name;
	t.isEmpty = false;
	t.initialized = true;

	for (const ThemeRow& r : rows)
	{
		if (r.type == "imvec4")
		{
			int idx = ColorIndexByName(r.name);
			if (idx < 0)
			{
				continue;
			}
			std::vector<float> f = ParseFloats(r.value);
			if (f.size() >= 4)
			{
				t.colors[idx] = ImVec4(f[0], f[1], f[2], f[3]);
			}
		}
		else if (r.type == "imvec2")
		{
			std::vector<float> f = ParseFloats(r.value);
			if (f.size() < 2)
			{
				continue;
			}
			if (const Vec2Field* field = FindVec2Field(r.name))
			{
				t.*field->t = ImVec2(f[0], f[1]);
			}
		}
		else
		{
			// Scalars: parse the value as float regardless of the row's stored
			// type so legacy border-size rows saved as "bool" (0/1) still load.
			std::vector<float> f = ParseFloats(r.value);
			if (f.empty())
			{
				continue;
			}
			if (const ScalarField* field = FindScalarField(r.name))
			{
				t.*field->t = f[0];
			}
		}
	}

	return t;
}

void ApplySharedStyle(Theme& t)
{
	for (const ScalarField& f : kScalarFields)
	{
		t.*f.t = f.def;
	}
	for (const Vec2Field& f : kVec2Fields)
	{
		t.*f.t = f.def;
	}
}

// Byte-identical derived/default tail shared by every builtin theme. Call after
// the per-accent literals so the Lerp inputs (Header/HeaderActive/TitleBg*) are set.
void FinishBuiltin(ImVec4* c)
{
	c[ImGuiCol_Tab] = myui::LerpColor(c[ImGuiCol_Header], c[ImGuiCol_TitleBgActive], 0.80f);
	c[ImGuiCol_TabHovered] = c[ImGuiCol_HeaderHovered];
	c[ImGuiCol_TabSelected] = myui::LerpColor(c[ImGuiCol_HeaderActive], c[ImGuiCol_TitleBgActive], 0.60f);
	c[ImGuiCol_TabDimmed] = myui::LerpColor(c[ImGuiCol_Tab], c[ImGuiCol_TitleBg], 0.80f);
	c[ImGuiCol_TabDimmedSelected] = myui::LerpColor(c[ImGuiCol_TabSelected], c[ImGuiCol_TitleBg], 0.40f);
	{
		const ImVec4& ha = c[ImGuiCol_HeaderActive];
		c[ImGuiCol_DockingPreview] = ImVec4(ha.x, ha.y, ha.z, ha.w * 0.70f);
	}
	c[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	c[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	c[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	c[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	c[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
}
} // namespace

void ThemeManager::Init(SettingsStore* store)
{
	m_store = store;
	CaptureImGuiDefault();
	LoadAll();
}

void ThemeManager::CaptureImGuiDefault()
{
	m_defaultTheme = Theme{};
	m_defaultTheme.name = "Default";
	m_defaultTheme.initialized = true;

	const ImGuiStyle& s = ImGui::GetStyle();
	for (int i = 0; i < ImGuiCol_COUNT; ++i)
	{
		m_defaultTheme.colors[i] = s.Colors[i];
	}

	for (const ScalarField& f : kScalarFields)
	{
		m_defaultTheme.*f.t = s.*f.s;
	}
	for (const Vec2Field& f : kVec2Fields)
	{
		m_defaultTheme.*f.t = s.*f.s;
	}
}

void ThemeManager::EnsureDefaultSentinel()
{
	Theme d;
	d.name = "Default";
	d.isEmpty = true;
	d.initialized = true;
	m_themes["Default"] = d;
}

void ThemeManager::SeedBuiltins()
{
	if (!m_store)
	{
		return;
	}
	if (m_store->GetGlobal("ThemeZ", "SeededBuiltins", "") == "1")
	{
		return;
	}

	m_store->SaveTheme("Grape", ToRows(MakeGrape()));
	m_store->SaveTheme("Burnt", ToRows(MakeBurnt()));
	m_store->SetGlobal("ThemeZ", "SeededBuiltins", "1");
}

void ThemeManager::LoadAll()
{
	m_themes.clear();

	if (m_store)
	{
		SeedBuiltins();
		for (const std::string& name : m_store->GetThemeNames())
		{
			if (name == "Default")
			{
				continue;
			}
			std::vector<ThemeRow> rows = m_store->LoadTheme(name);
			m_themes[name] = FromRows(name, rows, m_defaultTheme);
		}
	}

	EnsureDefaultSentinel();
}

const Theme* ThemeManager::GetTheme(const std::string& name) const
{
	auto it = m_themes.find(name);
	if (it != m_themes.end() && it->second.initialized)
	{
		return &it->second;
	}

	auto def = m_themes.find("Default");
	if (def != m_themes.end())
	{
		return &def->second;
	}

	return nullptr;
}

std::vector<std::string> ThemeManager::GetThemeNames() const
{
	std::vector<std::string> names;
	names.reserve(m_themes.size());
	names.push_back("Default");
	for (const auto& [name, theme] : m_themes)
	{
		if (name != "Default")
		{
			names.push_back(name);
		}
	}
	return names;
}

ImGuiStyle ThemeManager::ApplyTheme(const Theme& theme) const
{
	ImGuiStyle old = ImGui::GetStyle();
	if (theme.isEmpty)
	{
		return old;
	}

	ImGuiStyle& style = ImGui::GetStyle();
	for (int i = 0; i < ImGuiCol_COUNT; ++i)
	{
		style.Colors[i] = theme.colors[i];
	}

	for (const ScalarField& f : kScalarFields)
	{
		style.*f.s = theme.*f.t;
	}
	for (const Vec2Field& f : kVec2Fields)
	{
		style.*f.s = theme.*f.t;
	}

	return old;
}

void ThemeManager::ResetTheme(const ImGuiStyle& old)
{
	ImGui::GetStyle() = old;
}

void ThemeManager::SaveTheme(const Theme& theme)
{
	if (theme.name.empty() || theme.name == "Default")
	{
		return;
	}

	Theme stored = theme;
	stored.isEmpty = false;
	stored.initialized = true;
	m_themes[theme.name] = stored;

	if (m_store)
	{
		m_store->SaveTheme(theme.name, ToRows(stored));
	}
}

void ThemeManager::DeleteTheme(const std::string& name)
{
	if (name == "Default")
	{
		return;
	}

	m_themes.erase(name);
	if (m_store)
	{
		m_store->DeleteTheme(name);
	}

	if (m_activeName == name)
	{
		SetActiveTheme("Default");
	}
}

void ThemeManager::SetActiveTheme(const std::string& name)
{
	m_activeName = name;
	if (m_store)
	{
		m_store->SetString("ThemeZ", "ActiveTheme", name);
	}
}

void ThemeManager::LoadActiveTheme()
{
	if (m_store)
	{
		m_activeName = m_store->GetString("ThemeZ", "ActiveTheme", "Default");
	}
	else
	{
		m_activeName = "Default";
	}
}

const Theme& ThemeManager::Resolved() const
{
	if (m_preview)
	{
		return *m_preview;
	}

	auto it = m_themes.find(m_activeName);
	if (it != m_themes.end() && it->second.initialized)
	{
		return it->second;
	}

	return m_themes.at("Default");
}

Theme ThemeManager::MakeGrape() const
{
	Theme t = m_defaultTheme;
	t.name = "Grape";
	t.isEmpty = false;
	t.initialized = true;
	ImVec4* c = t.colors;

	c[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	c[ImGuiCol_WindowBg] = ImVec4(0.0168f, 0.0022f, 0.0474f, 0.9400f);
	c[ImGuiCol_ChildBg] = ImVec4(0.0630f, 0.0059f, 0.1564f, 0.0000f);
	c[ImGuiCol_PopupBg] = ImVec4(0.0543f, 0.0119f, 0.1564f, 0.9400f);
	c[ImGuiCol_Border] = ImVec4(0.2946f, 0.1528f, 0.3981f, 0.5000f);
	c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_FrameBg] = ImVec4(0.2631f, 0.1600f, 0.4800f, 0.5400f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.4306f, 0.2600f, 0.9800f, 0.4000f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.5334f, 0.2600f, 0.9800f, 0.6700f);
	c[ImGuiCol_TitleBg] = ImVec4(0.1031f, 0.0037f, 0.1943f, 1.00f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.3544f, 0.1600f, 0.4800f, 1.00f);
	c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0788f, 0.0133f, 0.1857f, 0.5100f);
	c[ImGuiCol_MenuBarBg] = ImVec4(0.1400f, 0.1400f, 0.1400f, 1.00f);
	c[ImGuiCol_ScrollbarBg] = ImVec4(0.0200f, 0.0200f, 0.0200f, 0.5300f);
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.4585f, 0.1383f, 0.6209f, 1.00f);
	c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.4136f, 0.2610f, 0.6635f, 1.00f);
	c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.6160f, 0.1153f, 0.8389f, 1.00f);
	c[ImGuiCol_CheckMark] = ImVec4(0.6994f, 0.5758f, 0.9283f, 1.00f);
	c[ImGuiCol_SliderGrab] = ImVec4(0.5965f, 0.2400f, 0.8800f, 1.00f);
	c[ImGuiCol_SliderGrabActive] = ImVec4(0.5516f, 0.2600f, 0.9800f, 1.00f);
	c[ImGuiCol_Button] = ImVec4(0.2313f, 0.1024f, 0.7204f, 0.4000f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.5739f, 0.2600f, 0.9800f, 1.00f);
	c[ImGuiCol_ButtonActive] = ImVec4(0.4792f, 0.0600f, 0.9800f, 1.00f);
	c[ImGuiCol_Header] = ImVec4(0.5699f, 0.2600f, 0.9800f, 0.3100f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.6428f, 0.2600f, 0.9800f, 0.8000f);
	c[ImGuiCol_HeaderActive] = ImVec4(0.5739f, 0.2600f, 0.9800f, 1.00f);
	c[ImGuiCol_Separator] = c[ImGuiCol_Border];
	c[ImGuiCol_SeparatorHovered] = ImVec4(0.7500f, 0.3588f, 0.1000f, 0.7800f);
	c[ImGuiCol_SeparatorActive] = ImVec4(0.7500f, 0.3033f, 0.1000f, 1.00f);
	c[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	c[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	c[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	FinishBuiltin(c);
	c[ImGuiCol_TableHeaderBg] = ImVec4(0.2259f, 0.1795f, 0.2559f, 1.00f);
	c[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	c[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.70f);
	c[ImGuiCol_TableRowBgAlt] = ImVec4(0.20f, 0.22f, 0.27f, 0.80f);
	c[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	c[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	c[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ApplySharedStyle(t);
	return t;
}

Theme ThemeManager::MakeBurnt() const
{
	Theme t = m_defaultTheme;
	t.name = "Burnt";
	t.isEmpty = false;
	t.initialized = true;
	ImVec4* c = t.colors;

	c[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	c[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	c[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	c[ImGuiCol_Border] = ImVec4(0.96f, 0.47f, 0.06f, 0.50f);
	c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_FrameBg] = ImVec4(0.25f, 0.24f, 0.23f, 0.54f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.98f, 0.69f, 0.26f, 0.40f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
	c[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	c[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	c[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	c[ImGuiCol_CheckMark] = ImVec4(0.98f, 0.53f, 0.26f, 1.00f);
	c[ImGuiCol_SliderGrab] = ImVec4(0.88f, 0.46f, 0.24f, 1.00f);
	c[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.55f, 0.26f, 1.00f);
	c[ImGuiCol_Button] = ImVec4(0.67f, 0.35f, 0.19f, 0.40f);
	c[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.58f, 0.00f, 1.00f);
	c[ImGuiCol_ButtonActive] = ImVec4(0.98f, 0.40f, 0.06f, 1.00f);
	c[ImGuiCol_Header] = ImVec4(0.98f, 0.55f, 0.26f, 0.31f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.98f, 0.69f, 0.26f, 0.80f);
	c[ImGuiCol_HeaderActive] = ImVec4(0.98f, 0.73f, 0.26f, 1.00f);
	c[ImGuiCol_Separator] = ImVec4(1.00f, 0.28f, 0.00f, 0.50f);
	c[ImGuiCol_SeparatorHovered] = ImVec4(0.75f, 0.36f, 0.10f, 0.78f);
	c[ImGuiCol_SeparatorActive] = ImVec4(0.75f, 0.30f, 0.10f, 1.00f);
	c[ImGuiCol_ResizeGrip] = ImVec4(0.95f, 0.55f, 0.09f, 0.20f);
	c[ImGuiCol_ResizeGripHovered] = ImVec4(0.98f, 0.55f, 0.26f, 0.67f);
	c[ImGuiCol_ResizeGripActive] = ImVec4(0.98f, 0.61f, 0.26f, 0.95f);
	FinishBuiltin(c);
	c[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	c[ImGuiCol_TableBorderStrong] = ImVec4(1.00f, 0.46f, 0.00f, 1.00f);
	c[ImGuiCol_TableBorderLight] = ImVec4(1.00f, 0.46f, 0.00f, 1.00f);
	c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	c[ImGuiCol_TextSelectedBg] = ImVec4(0.98f, 0.73f, 0.26f, 0.35f);
	c[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	c[ImGuiCol_NavCursor] = ImVec4(0.91f, 0.09f, 0.21f, 1.00f);
	c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ApplySharedStyle(t);
	return t;
}
