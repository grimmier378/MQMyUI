#include "ThemeZModule.h"

#include "../Core/UiConfig.h"
#include "../Core/Widgets.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace themeeditor
{
void LoadSelected(ThemeManager* themes, const std::string& name, Theme& edit, std::string& selected, char* nameBuf, size_t nameBufSize)
{
	if (!themes)
	{
		return;
	}

	selected = name;

	if (name == "Default")
	{
		edit = themes->DefaultStyle();
		edit.name = "Default";
	}
	else if (const Theme* t = themes->GetTheme(name))
	{
		edit = *t;
	}

	strncpy_s(nameBuf, nameBufSize, edit.name.c_str(), _TRUNCATE);
}

void DrawTopBar(ThemeManager* themes, Theme& edit, std::string& selected, char* nameBuf, size_t nameBufSize)
{
	ImGui::TextDisabled("Active: %s", themes->ActiveName().c_str());

	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("Edit Theme", selected.c_str()))
	{
		for (const std::string& n : themes->GetThemeNames())
		{
			bool isSelected = (n == selected);
			if (ImGui::Selectable(n.c_str(), isSelected))
			{
				LoadSelected(themes, n, edit, selected, nameBuf, nameBufSize);
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SetNextItemWidth(200.0f);
	myui::StyledEditField("Name", nameBuf, nameBufSize);

	const bool isDefaultSelected = (selected == "Default");
	const std::string targetName = nameBuf;
	const bool canSave = !targetName.empty() && targetName != "Default";

	if (myui::StyledButton("New"))
	{
		std::string base = "New Theme";
		std::string name = base;
		int counter = 2;
		auto exists = [&](const std::string& nm) {
			for (const std::string& e : themes->GetThemeNames())
			{
				if (e == nm)
				{
					return true;
				}
			}
			return false;
		};
		while (exists(name))
		{
			name = fmt::format("{} {}", base, counter++);
		}

		Theme dup = edit;
		dup.name = name;
		dup.isEmpty = false;
		themes->SaveTheme(dup);
		LoadSelected(themes, name, edit, selected, nameBuf, nameBufSize);
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!canSave);
	if (myui::StyledButton("Save"))
	{
		edit.name = targetName;
		themes->SaveTheme(edit);
		selected = targetName;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(isDefaultSelected);
	if (myui::StyledButton("Delete"))
	{
		themes->DeleteTheme(selected);
		LoadSelected(themes, "Default", edit, selected, nameBuf, nameBufSize);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (myui::StyledButton("Apply"))
	{
		themes->SetActiveTheme(selected);
	}

	ImGui::SameLine();
	if (myui::StyledButton("Revert"))
	{
		LoadSelected(themes, selected, edit, selected, nameBuf, nameBufSize);
	}
}

void DrawColorsSection(ThemeManager* themes, Theme& edit)
{
	if (!ImGui::CollapsingHeader("Colors"))
	{
		return;
	}

	if (myui::StyledButton("Reset Colors"))
	{
		const Theme& d = themes->DefaultStyle();
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			edit.colors[i] = d.colors[i];
		}
	}

	std::vector<int> order;
	order.reserve(ImGuiCol_COUNT);
	for (int i = 0; i < ImGuiCol_COUNT; ++i)
	{
		order.push_back(i);
	}
	std::sort(order.begin(), order.end(), [](int a, int b) {
		return std::strcmp(ImGui::GetStyleColorName(a), ImGui::GetStyleColorName(b)) < 0;
	});

	for (int i : order)
	{
		ImGui::ColorEdit4(ImGui::GetStyleColorName(i), reinterpret_cast<float*>(&edit.colors[i]),
			ImGuiColorEditFlags_AlphaBar);
	}
}

void DrawStylesSection(ThemeManager* themes, Theme& edit)
{
	if (!ImGui::CollapsingHeader("Styles"))
	{
		return;
	}

	if (myui::StyledButton("Reset Styles"))
	{
		const Theme& d = themes->DefaultStyle();
		edit.windowRounding    = d.windowRounding;
		edit.frameRounding     = d.frameRounding;
		edit.childRounding     = d.childRounding;
		edit.popupRounding     = d.popupRounding;
		edit.scrollbarRounding = d.scrollbarRounding;
		edit.grabRounding      = d.grabRounding;
		edit.tabRounding       = d.tabRounding;
		edit.windowBorderSize  = d.windowBorderSize;
		edit.frameBorderSize   = d.frameBorderSize;
		edit.childBorderSize   = d.childBorderSize;
		edit.popupBorderSize   = d.popupBorderSize;
		edit.tabBarBorderSize  = d.tabBarBorderSize;
		edit.indentSpacing     = d.indentSpacing;
		edit.scrollbarSize     = d.scrollbarSize;
		edit.grabMinSize       = d.grabMinSize;
		edit.windowPadding     = d.windowPadding;
		edit.framePadding      = d.framePadding;
		edit.cellPadding       = d.cellPadding;
		edit.itemSpacing       = d.itemSpacing;
		edit.itemInnerSpacing  = d.itemInnerSpacing;
	}

	auto boolEdit = [](const char* label, float& field) {
		bool b = field > 0.5f;
		if (ImGui::Checkbox(label, &b))
		{
			field = b ? 1.0f : 0.0f;
		}
	};

	ImGui::SeparatorText("Borders");
	boolEdit("WindowBorderSize", edit.windowBorderSize);
	boolEdit("FrameBorderSize", edit.frameBorderSize);
	boolEdit("ChildBorderSize", edit.childBorderSize);
	boolEdit("PopupBorderSize", edit.popupBorderSize);
	boolEdit("TabBarBorderSize", edit.tabBarBorderSize);

	auto v2Edit = [](const char* label, ImVec2& v) {
		float a[2] = { v.x, v.y };
		if (ImGui::DragFloat2(label, a, 0.1f, 0.0f, 40.0f, "%.0f"))
		{
			v.x = a[0];
			v.y = a[1];
		}
	};

	ImGui::SeparatorText("Spacing");
	v2Edit("WindowPadding", edit.windowPadding);
	v2Edit("FramePadding", edit.framePadding);
	v2Edit("CellPadding", edit.cellPadding);
	v2Edit("ItemSpacing", edit.itemSpacing);
	v2Edit("ItemInnerSpacing", edit.itemInnerSpacing);
	ImGui::SliderFloat("IndentSpacing", &edit.indentSpacing, 0.0f, 30.0f, "%.0f");
	ImGui::SliderFloat("ScrollbarSize", &edit.scrollbarSize, 0.0f, 20.0f, "%.0f");
	ImGui::SliderFloat("GrabMinSize", &edit.grabMinSize, 0.0f, 20.0f, "%.0f");

	ImGui::SeparatorText("Rounding");
	ImGui::SliderFloat("WindowRounding", &edit.windowRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("FrameRounding", &edit.frameRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("ChildRounding", &edit.childRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("PopupRounding", &edit.popupRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("ScrollbarRounding", &edit.scrollbarRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("GrabRounding", &edit.grabRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("TabRounding", &edit.tabRounding, 0.0f, 12.0f, "%.0f");
}
} // namespace themeeditor

void ThemeZModule::OnShutdown()
{
	if (m_ctx.Themes)
	{
		m_ctx.Themes->ClearPreview();
	}
}

void ThemeZModule::OnRenderGUI()
{
	ThemeManager* themes = m_ctx.Themes;
	WindowConfig& w = m_ctx.UI->Window(GetName());

	if (!w.visible)
	{
		if (themes && m_wasOpen)
		{
			themes->ClearPreview();
		}
		m_wasOpen = false;
		return;
	}

	if (!themes)
	{
		return;
	}

	if (!m_wasOpen)
	{
		themeeditor::LoadSelected(themes, themes->ActiveName(), m_edit, m_selected, m_nameBuf, sizeof(m_nameBuf));
		m_wasOpen = true;
	}

	themes->SetPreview(&m_edit);

	ImGui::SetNextWindowSize(ImVec2(430.0f, 500.0f), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_None;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove;
	}

	if (ImGui::Begin("MyUI Themes##MyUIThemes", &w.visible, flags))
	{
		themeeditor::DrawTopBar(themes, m_edit, m_selected, m_nameBuf, sizeof(m_nameBuf));
		ImGui::Separator();
		themeeditor::DrawColorsSection(themes, m_edit);
		themeeditor::DrawStylesSection(themes, m_edit);
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}
}
