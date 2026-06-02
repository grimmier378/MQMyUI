#include "ThemeZModule.h"

#include "../Core/UiConfig.h"

#include <algorithm>
#include <cstring>
#include <vector>

void ThemeZModule::OnShutdown()
{
	if (m_ctx.Themes)
	{
		m_ctx.Themes->ClearPreview();
	}
}

void ThemeZModule::LoadSelected(const std::string& name)
{
	if (!m_ctx.Themes)
	{
		return;
	}

	m_selected = name;

	if (name == "Default")
	{
		m_edit = m_ctx.Themes->DefaultStyle();
		m_edit.name = "Default";
	}
	else if (const Theme* t = m_ctx.Themes->GetTheme(name))
	{
		m_edit = *t;
	}

	strncpy_s(m_nameBuf, m_edit.name.c_str(), _TRUNCATE);
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
		LoadSelected(themes->ActiveName());
		m_wasOpen = true;
	}

	themes->SetPreview(&m_edit);

	ImGui::SetNextWindowSize(ImVec2(430.0f, 500.0f), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_None;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove;
	}

	if (ImGui::Begin("MyUI ThemeZ##MyUIThemeZ", &w.visible, flags))
	{
		DrawTopBar();
		ImGui::Separator();
		DrawColorsSection();
		DrawStylesSection();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}
}

void ThemeZModule::DrawTopBar()
{
	ThemeManager* themes = m_ctx.Themes;

	ImGui::TextDisabled("Active: %s", themes->ActiveName().c_str());

	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("Edit Theme", m_selected.c_str()))
	{
		for (const std::string& n : themes->GetThemeNames())
		{
			bool selected = (n == m_selected);
			if (ImGui::Selectable(n.c_str(), selected))
			{
				LoadSelected(n);
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SetNextItemWidth(200.0f);
	ImGui::InputText("Name", m_nameBuf, sizeof(m_nameBuf));

	const bool isDefaultSelected = (m_selected == "Default");
	const std::string targetName = m_nameBuf;
	const bool canSave = !targetName.empty() && targetName != "Default";

	if (ImGui::Button("New"))
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

		Theme dup = m_edit;
		dup.name = name;
		dup.isEmpty = false;
		themes->SaveTheme(dup);
		LoadSelected(name);
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!canSave);
	if (ImGui::Button("Save"))
	{
		m_edit.name = targetName;
		themes->SaveTheme(m_edit);
		m_selected = targetName;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(isDefaultSelected);
	if (ImGui::Button("Delete"))
	{
		themes->DeleteTheme(m_selected);
		LoadSelected("Default");
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Apply"))
	{
		themes->SetActiveTheme(m_selected);
	}

	ImGui::SameLine();
	if (ImGui::Button("Revert"))
	{
		LoadSelected(m_selected);
	}
}

void ThemeZModule::DrawColorsSection()
{
	if (!ImGui::CollapsingHeader("Colors"))
	{
		return;
	}

	if (ImGui::Button("Reset Colors"))
	{
		const Theme& d = m_ctx.Themes->DefaultStyle();
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			m_edit.colors[i] = d.colors[i];
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
		ImGui::ColorEdit4(ImGui::GetStyleColorName(i), reinterpret_cast<float*>(&m_edit.colors[i]),
			ImGuiColorEditFlags_AlphaBar);
	}
}

void ThemeZModule::DrawStylesSection()
{
	if (!ImGui::CollapsingHeader("Styles"))
	{
		return;
	}

	if (ImGui::Button("Reset Styles"))
	{
		const Theme& d = m_ctx.Themes->DefaultStyle();
		m_edit.windowRounding    = d.windowRounding;
		m_edit.frameRounding     = d.frameRounding;
		m_edit.childRounding     = d.childRounding;
		m_edit.popupRounding     = d.popupRounding;
		m_edit.scrollbarRounding = d.scrollbarRounding;
		m_edit.grabRounding      = d.grabRounding;
		m_edit.tabRounding       = d.tabRounding;
		m_edit.windowBorderSize  = d.windowBorderSize;
		m_edit.frameBorderSize   = d.frameBorderSize;
		m_edit.childBorderSize   = d.childBorderSize;
		m_edit.popupBorderSize   = d.popupBorderSize;
		m_edit.tabBarBorderSize  = d.tabBarBorderSize;
		m_edit.indentSpacing     = d.indentSpacing;
		m_edit.scrollbarSize     = d.scrollbarSize;
		m_edit.grabMinSize       = d.grabMinSize;
		m_edit.windowPadding     = d.windowPadding;
		m_edit.framePadding      = d.framePadding;
		m_edit.cellPadding       = d.cellPadding;
		m_edit.itemSpacing       = d.itemSpacing;
		m_edit.itemInnerSpacing  = d.itemInnerSpacing;
	}

	auto boolEdit = [](const char* label, float& field) {
		bool b = field > 0.5f;
		if (ImGui::Checkbox(label, &b))
		{
			field = b ? 1.0f : 0.0f;
		}
	};

	ImGui::SeparatorText("Borders");
	boolEdit("WindowBorderSize", m_edit.windowBorderSize);
	boolEdit("FrameBorderSize", m_edit.frameBorderSize);
	boolEdit("ChildBorderSize", m_edit.childBorderSize);
	boolEdit("PopupBorderSize", m_edit.popupBorderSize);
	boolEdit("TabBarBorderSize", m_edit.tabBarBorderSize);

	auto v2Edit = [](const char* label, ImVec2& v) {
		float a[2] = { v.x, v.y };
		if (ImGui::DragFloat2(label, a, 0.1f, 0.0f, 40.0f, "%.0f"))
		{
			v.x = a[0];
			v.y = a[1];
		}
	};

	ImGui::SeparatorText("Spacing");
	v2Edit("WindowPadding", m_edit.windowPadding);
	v2Edit("FramePadding", m_edit.framePadding);
	v2Edit("CellPadding", m_edit.cellPadding);
	v2Edit("ItemSpacing", m_edit.itemSpacing);
	v2Edit("ItemInnerSpacing", m_edit.itemInnerSpacing);
	ImGui::SliderFloat("IndentSpacing", &m_edit.indentSpacing, 0.0f, 30.0f, "%.0f");
	ImGui::SliderFloat("ScrollbarSize", &m_edit.scrollbarSize, 0.0f, 20.0f, "%.0f");
	ImGui::SliderFloat("GrabMinSize", &m_edit.grabMinSize, 0.0f, 20.0f, "%.0f");

	ImGui::SeparatorText("Rounding");
	ImGui::SliderFloat("WindowRounding", &m_edit.windowRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("FrameRounding", &m_edit.frameRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("ChildRounding", &m_edit.childRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("PopupRounding", &m_edit.popupRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("ScrollbarRounding", &m_edit.scrollbarRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("GrabRounding", &m_edit.grabRounding, 0.0f, 12.0f, "%.0f");
	ImGui::SliderFloat("TabRounding", &m_edit.tabRounding, 0.0f, 12.0f, "%.0f");
}
