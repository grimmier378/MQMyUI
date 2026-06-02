#include "SettingsModule.h"

#include "../Core/UiConfig.h"
#include "../Core/Widgets.h"
#include "../Core/ThemeManager.h"
#include "../Core/ActorManager.h"
#include "../Core/PeerData.h"

#include "mq/imgui/ImGuiUtils.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
const char* kDisplayWindows[] = {
	"Player", "Pet", "Group", "Raid", "Buffs", "Songs", "Spells", "Casting", "XPBars", "MyAA", "HUD", "MyInventory", "BigBag", "iTrack"
};

const char* kScalingWindows[] = {
	"Player", "Pet", "Group", "Raid", "Buffs", "Songs", "Spells", "Casting", "XPBars", "MyAA", "HUD", "MyInventory", "BigBag", "iTrack", "Toolbar"
};

const char* kHotkeyWindows[] = { "MyInventory", "BigBag", "iTrack" };
const char* kModOptions[] = { "None", "Ctrl", "Alt", "Shift" };
const char* kMouseOptions[] = { "None", "Middle" };

const std::vector<std::string>& KeyOptions()
{
	static std::vector<std::string> options = [] {
		std::vector<std::string> v;
		v.push_back("None");
		for (char c = 'A'; c <= 'Z'; ++c)
		{
			v.push_back(std::string(1, c));
		}
		for (char c = '0'; c <= '9'; ++c)
		{
			v.push_back(std::string(1, c));
		}
		for (int i = 1; i <= 12; ++i)
		{
			v.push_back("F" + std::to_string(i));
		}
		return v;
	}();
	return options;
}

void OptionCombo(const char* label, std::string& value, const char* const* options, int count)
{
	const char* current = value.empty() ? "None" : value.c_str();
	ImGui::SetNextItemWidth(90.0f);
	if (ImGui::BeginCombo(label, current))
	{
		for (int i = 0; i < count; ++i)
		{
			bool selected = value == options[i];
			if (ImGui::Selectable(options[i], selected))
			{
				value = options[i];
			}
		}
		ImGui::EndCombo();
	}
}

void DrawKeyCombo(const char* label, std::string& value)
{
	const char* current = value.empty() ? "None" : value.c_str();
	ImGui::SetNextItemWidth(90.0f);
	if (ImGui::BeginCombo(label, current))
	{
		for (const std::string& option : KeyOptions())
		{
			bool isNone = option == "None";
			bool selected = isNone ? value.empty() : value == option;
			if (ImGui::Selectable(option.c_str(), selected))
			{
				value = isNone ? "" : option;
			}
		}
		ImGui::EndCombo();
	}
}

int ComposeToggleFlags(UiConfig* ui)
{
	int f = 0;
	if (ui->Flag("Toggles", "StarKnob", false))      { f |= myui::ToggleFlags_StarKnob; }
	if (ui->Flag("Toggles", "RightLabel", true))     { f |= myui::ToggleFlags_RightLabel; }
	if (ui->Flag("Toggles", "AnimateKnob", false))   { f |= myui::ToggleFlags_AnimateKnob; }
	if (ui->Flag("Toggles", "GlowOnHover", true))    { f |= myui::ToggleFlags_GlowOnHover; }
	if (ui->Flag("Toggles", "AnimateOnHover", false)){ f |= myui::ToggleFlags_AnimateOnHover; }
	if (ui->Flag("Toggles", "PulseOnHover", true))   { f |= myui::ToggleFlags_PulseOnHover; }
	if (ui->Flag("Toggles", "KnobBorder", false))    { f |= myui::ToggleFlags_KnobBorder; }
	return f;
}

bool ColorEditMQ(const char* label, MQColor& c)
{
	float v[4] = { c.Red / 255.0f, c.Green / 255.0f, c.Blue / 255.0f, c.Alpha / 255.0f };
	if (ImGui::ColorEdit4(label, v, ImGuiColorEditFlags_AlphaBar))
	{
		c = MQColor((uint8_t)(v[0] * 255.0f), (uint8_t)(v[1] * 255.0f), (uint8_t)(v[2] * 255.0f), (uint8_t)(v[3] * 255.0f));
		return true;
	}
	return false;
}
} // namespace

void SettingsModule::OnRenderGUI()
{
	UiConfig* local = m_ctx.UI;
	if (!local)
	{
		return;
	}

	WindowConfig& w = local->Window(GetName());
	if (!w.visible)
	{
		m_remoteActive = false;
		return;
	}

	myui::SetGlobalToggleStyle(ComposeToggleFlags(local),
		ImVec2(local->Num("Toggles", "Width", 0.0f), local->Num("Toggles", "Height", 0.0f)));

	m_active = m_remoteActive ? &m_remote : local;

	int pushedColors = 0;
	if (m_remoteActive)
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.55f, 0.30f, 0.05f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.35f, 0.20f, 0.05f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.6f, 0.2f, 1.0f));
		pushedColors = 3;
	}

	std::string title = m_remoteActive
		? ("MyUI Settings - Editing " + m_targetChar + "###MyUISettings")
		: std::string("MyUI Settings###MyUISettings");

	ImGui::SetNextWindowSize(ImVec2(470.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &w.visible))
	{
		DrawTargetPicker();

		if (m_remoteActive)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
				"Editing %s (remote) - Save pushes to that character.", m_targetChar.c_str());
		}

		if (ImGui::Button("Save Settings"))
		{
			if (m_remoteActive)
			{
				m_remote.SaveForCharacter(m_ctx.Settings, m_targetServer, m_targetChar);
				if (m_ctx.Actors)
				{
					m_ctx.Actors->SendCommand(m_targetServer, m_targetChar, "ReloadSettings");
				}
			}
			else
			{
				local->RequestSave();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Revert"))
		{
			if (m_remoteActive)
			{
				m_remote.LoadForCharacter(m_ctx.Settings, m_targetServer, m_targetChar);
			}
			else
			{
				local->RequestReload();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy Settings..."))
		{
			local->Window("Copy").visible = true;
			local->PersistVisibility("Copy");
		}

		ImGui::Separator();

		if (ImGui::BeginTabBar("##SettingsTabs"))
		{
			if (ImGui::BeginTabItem("Windows"))   { DrawWindowsTab();  ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Bars"))       { DrawBarsTab();     ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Scaling"))    { DrawScalingTab();  ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Display"))    { DrawDisplayTab();  ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Toggles"))    { DrawTogglesTab();  ImGui::EndTabItem(); }
			if (!m_remoteActive && ImGui::BeginTabItem("Theme")) { DrawThemeTab(); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	if (pushedColors > 0)
	{
		ImGui::PopStyleColor(pushedColors);
	}

	if (!w.visible)
	{
		local->PersistVisibility(GetName());
		local->Window("Copy").visible = false;
		local->previewCasting = false;
		m_remoteActive = false;
	}
}

void SettingsModule::DrawTargetPicker()
{
	std::string current = m_remoteActive ? (m_targetServer + " / " + m_targetChar) : std::string("This Character");
	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo("Editing##tgt", current.c_str()))
	{
		if (ImGui::Selectable("This Character", !m_remoteActive))
		{
			m_remoteActive = false;
		}
		if (m_ctx.Actors)
		{
			for (const auto& [key, peer] : m_ctx.Actors->Peers())
			{
				bool sel = m_remoteActive && ci_equals(peer.character, m_targetChar);
				std::string label = peer.server + " / " + peer.character;
				if (ImGui::Selectable(label.c_str(), sel))
				{
					m_targetServer = peer.server;
					m_targetChar = peer.character;
					m_remoteActive = true;
					m_remote.LoadForCharacter(m_ctx.Settings, m_targetServer, m_targetChar);
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	mq::imgui::HelpMarker("Edit another online character's settings. Save pushes to that character and reloads it.");
}

void SettingsModule::DrawWindowsTab()
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;
	if (ImGui::BeginTable("##WinTable", 3, ImGuiTableFlags_SizingFixedFit))
	{
		for (const char* name : kDisplayWindows)
		{
			WindowConfig& w = ui->Window(name);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(name);
			ImGui::TableNextColumn();
			if (myui::DrawToggle((std::string("Shown##vis") + name).c_str(), &w.visible))
			{
				ui->PersistVisibility(name);
			}
			ImGui::TableNextColumn();
			if (myui::DrawToggle((std::string("Locked##lock") + name).c_str(), &w.locked))
			{
				ui->PersistField(name, "Locked");
			}
		}
		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Window Hotkeys");
	ImGui::SameLine();
	mq::imgui::HelpMarker("Toggle a window with up to 3 modifiers plus a key, and/or the middle mouse button. Set unused fields to None.");

	for (const char* name : kHotkeyWindows)
	{
		WindowConfig& w = ui->Window(name);
		if (ImGui::TreeNode(name))
		{
			std::string id = name;
			DrawKeyCombo((std::string("Key##k") + id).c_str(), w.strs["ToggleKey"]);
			ImGui::SameLine();
			OptionCombo((std::string("Mouse##mo") + id).c_str(), w.strs["ToggleMouse"], kMouseOptions, 2);
			OptionCombo((std::string("Mod 1##m1") + id).c_str(), w.strs["ToggleMod1"], kModOptions, 4);
			ImGui::SameLine();
			OptionCombo((std::string("Mod 2##m2") + id).c_str(), w.strs["ToggleMod2"], kModOptions, 4);
			ImGui::SameLine();
			OptionCombo((std::string("Mod 3##m3") + id).c_str(), w.strs["ToggleMod3"], kModOptions, 4);
			ImGui::TreePop();
		}
	}
}

void SettingsModule::DrawBarsTab()
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;

	ImGui::BeginChild("##BarTree", ImVec2(150.0f, 0.0f), ImGuiChildFlags_Borders);
	for (const char* name : kDisplayWindows)
	{
		WindowConfig& w = ui->Window(name);
		if (w.bars.empty())
		{
			continue;
		}
		if (ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto& [role, style] : w.bars)
			{
				bool selected = (m_barWindow == name && m_barRole == role);
				if (ImGui::Selectable((role + "##" + name).c_str(), selected))
				{
					m_barWindow = name;
					m_barRole = role;
				}
			}
			ImGui::TreePop();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("##BarEditor", ImVec2(0.0f, 0.0f));
	if (!m_barWindow.empty() && !m_barRole.empty())
	{
		DrawBarEditor(m_barWindow, m_barRole);
	}
	else
	{
		ImGui::TextDisabled("Select a bar on the left.");
	}
	ImGui::EndChild();
}

void SettingsModule::DrawBarEditor(const std::string& window, const std::string& role)
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;
	BarStyle& b = ui->Bar(window, role);

	ImGui::PushID(window.c_str());
	ImGui::PushID(role.c_str());

	ImGui::Text("%s / %s", window.c_str(), role.c_str());
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset Effects"))
	{
		b.gradientOn = false;
		b.border = false;
		b.ticksOn = false;
		b.shimmerOn = false;
		b.glowOn = false;
		b.tweenSeconds = 0.0f;
	}

	if (ImGui::Button("Copy this style to..."))
	{
		ImGui::OpenPopup("##CopyBarTo");
	}
	if (ImGui::BeginPopup("##CopyBarTo"))
	{
		for (const char* w2 : kDisplayWindows)
		{
			WindowConfig& wc = ui->Window(w2);
			for (auto& [r2, s2] : wc.bars)
			{
				if (std::string(w2) == window && r2 == role)
				{
					continue;
				}
				if (ImGui::Selectable((std::string(w2) + " / " + r2).c_str()))
				{
					s2 = b;
				}
			}
		}
		ImGui::EndPopup();
	}

	if (window == "Casting" && role == "Cast")
	{
		ImGui::SameLine();
		ImGui::Checkbox("Force Show", &ui->previewCasting);
	}

	ImGui::Separator();

	const bool groupPet = ((window == "Group" || window == "Raid") && role == "Pet");

	if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (!groupPet)
		{
			ImGui::Checkbox("Vertical", &b.vertical);
		}
		ImGui::DragFloat("Height", &b.height, 0.5f, 1.0f, 200.0f, "%.0f");
		ImGui::DragFloat("Width (0=auto)", &b.width, 0.5f, 0.0f, 400.0f, "%.0f");
		ImGui::DragFloat("Rounding", &b.rounding, 0.1f, 0.0f, 24.0f, "%.1f");
		ImGui::DragFloat("Pad End", &b.padEnd, 0.5f, 0.0f, 40.0f, "%.0f");
	}

	if (ImGui::CollapsingHeader("Fill", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ColorEditMQ("Fill Low", b.fillLow);
		ColorEditMQ("Fill High", b.fillHigh);
		ColorEditMQ("Background", b.bgColor);
		ImGui::Checkbox("Gradient", &b.gradientOn);
		if (b.gradientOn)
		{
			const char* modes[] = { "Static", "Dynamic" };
			ImGui::Combo("Gradient Mode", &b.gradientMode, modes, 2);
			const char* dirs[] = { "Horizontal", "Vertical", "Diag TL-BR", "Diag TR-BL" };
			ImGui::Combo("Gradient Dir", &b.gradientDir, dirs, 4);
		}
	}

	if (ImGui::CollapsingHeader("Text"))
	{
		const char* modes[] = { "None", "Percent", "Value" };
		ImGui::Combo("Text Mode", &b.textMode, modes, 3);
		if (b.textMode != 0)
		{
			if (b.textMode == 1)
			{
				char fmt[32];
				strncpy_s(fmt, b.textFormat.c_str(), _TRUNCATE);
				if (ImGui::InputText("Format", fmt, sizeof(fmt)))
				{
					b.textFormat = fmt;
				}
			}
			ImGui::DragFloat("Text Scale", &b.textScale, 0.01f, 0.3f, 3.0f, "%.2f");
		}
	}

	ImGui::Checkbox("Border", &b.border);
	if (b.border)
	{
		ImGui::Indent();
		ImGui::DragFloat("Border Thickness", &b.borderThickness, 0.1f, 0.5f, 6.0f, "%.1f");
		ColorEditMQ("Border Color", b.borderColor);
		ImGui::Unindent();
	}

	if (ImGui::CollapsingHeader("Efx"))
	{
		ImGui::Checkbox("Ticks", &b.ticksOn);
		if (b.ticksOn)
		{
			ImGui::Indent();
			ImGui::DragFloat("Tick Every (0-1)", &b.tickEvery, 0.01f, 0.01f, 1.0f, "%.2f");
			ImGui::DragFloat("Tick Thickness", &b.tickThickness, 0.1f, 0.5f, 4.0f, "%.1f");
			ImGui::DragInt("Tick Alpha", &b.tickAlpha, 1.0f, 0, 255);
			ImGui::Unindent();
		}

		ImGui::Checkbox("Shimmer", &b.shimmerOn);
		if (b.shimmerOn)
		{
			ImGui::Indent();
			ImGui::DragFloat("Shimmer Speed", &b.shimmerSpeed, 0.01f, 0.0f, 4.0f, "%.2f");
			ImGui::DragFloat("Shimmer Width", &b.shimmerWidth, 0.5f, 4.0f, 120.0f, "%.0f");
			ImGui::Checkbox("Follows Progress", &b.shimmerFollows);
			ImGui::Unindent();
		}

		ImGui::Checkbox("Glow", &b.glowOn);
		if (b.glowOn)
		{
			ImGui::Indent();
			ImGui::DragFloat("Glow Thickness", &b.glowThickness, 0.5f, 0.0f, 60.0f, "%.0f");
			ImGui::DragInt("Glow Alpha", &b.glowAlpha, 1.0f, 0, 255);
			ImGui::Unindent();
		}

		ImGui::SeparatorText("Animation");
		ImGui::DragFloat("Tween Seconds", &b.tweenSeconds, 0.01f, 0.0f, 2.0f, "%.2f");
	}

	ImGui::PopID();
	ImGui::PopID();
}

void SettingsModule::DrawScalingTab()
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;
	if (ImGui::BeginTable("##ScaleTable", 3, ImGuiTableFlags_SizingStretchProp))
	{
		for (const char* name : kScalingWindows)
		{
			WindowConfig& w = ui->Window(name);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(name);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat((std::string("Text##t") + name).c_str(), &w.textScale, 0.01f, 0.3f, 3.0f, "Text %.2f");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat((std::string("Icon##i") + name).c_str(), &w.iconScale, 0.01f, 0.3f, 3.0f, "Icon %.2f");
		}
		ImGui::EndTable();
	}
}

void SettingsModule::DrawDisplayTab()
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;

	auto excluded = [](const std::string& win, const std::string& flag) {
		if (win == "Pet" && (flag == "ShowButtons" || flag == "ShowBuffs"))
		{
			return true;
		}
		if (win == "Buffs" && flag == "ShowTimer")
		{
			return true;
		}
		if (win == "MyInventory" && ci_starts_with(flag, "BagLock"))
		{
			return true;
		}
		return false;
	};

	for (const char* name : kDisplayWindows)
	{
		WindowConfig& w = ui->Window(name);
		bool isRaid = ci_equals(name, "Raid");
		if (w.flags.empty() && !isRaid)
		{
			continue;
		}
		if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (!w.flags.empty())
			{
				float avail = ImGui::GetContentRegionAvail().x;
				int cols = (int)(avail / 150.0f);
				if (cols < 1)
				{
					cols = 1;
				}
				if (cols > 6)
				{
					cols = 6;
				}

				if (ImGui::BeginTable((std::string("##disp") + name).c_str(), cols, ImGuiTableFlags_SizingStretchSame))
				{
					for (auto& [flag, val] : w.flags)
					{
						if (excluded(name, flag))
						{
							continue;
						}
						ImGui::TableNextColumn();
						myui::DrawToggle((flag + "##" + name).c_str(), &val);
					}
					ImGui::EndTable();
				}
			}

			if (isRaid)
			{
				float tileWidth = ui->Num("Raid", "TileWidth", 180.0f);
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::DragFloat("Tile Width##RaidTW", &tileWidth, 1.0f, 80.0f, 600.0f, "%.0f"))
				{
					ui->SetNum("Raid", "TileWidth", tileWidth);
				}
			}
		}
	}
}

void SettingsModule::DrawTogglesTab()
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;

	ImGui::TextWrapped("Configure the look of every toggle switch in the UI.");
	ImGui::Separator();

	auto flagToggle = [&](const char* label, const char* key, bool def) {
		bool v = ui->Flag("Toggles", key, def);
		if (myui::DrawToggle((std::string(label) + "##tg" + key).c_str(), &v))
		{
			ui->SetFlag("Toggles", key, v);
		}
	};

	flagToggle("Right Label", "RightLabel", true);
	flagToggle("Glow on Hover", "GlowOnHover", true);
	flagToggle("Pulse on Hover", "PulseOnHover", true);
	flagToggle("Star / Moon Knob", "StarKnob", false);
	flagToggle("Knob Border", "KnobBorder", false);
	flagToggle("Animate Knob", "AnimateKnob", false);
	flagToggle("Animate on Hover", "AnimateOnHover", false);

	ImGui::Separator();

	float width = ui->Num("Toggles", "Width", 0.0f);
	float height = ui->Num("Toggles", "Height", 0.0f);
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::DragFloat("Width (0 = auto)##tgw", &width, 0.5f, 0.0f, 80.0f, "%.0f"))
	{
		ui->SetNum("Toggles", "Width", width);
	}
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::DragFloat("Height (0 = auto)##tgh", &height, 0.5f, 0.0f, 60.0f, "%.0f"))
	{
		ui->SetNum("Toggles", "Height", height);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Preview:");
	ImGui::SameLine();
	static bool s_previewOn = true;
	myui::DrawToggle("Sample##tgPreview", &s_previewOn);
}

void SettingsModule::DrawThemeTab()
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;
	ThemeManager* themes = m_ctx.Themes;
	if (!themes)
	{
		return;
	}

	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo("Active Theme", themes->ActiveName().c_str()))
	{
		for (const std::string& n : themes->GetThemeNames())
		{
			bool sel = (n == themes->ActiveName());
			if (ImGui::Selectable(n.c_str(), sel))
			{
				themes->SetActiveTheme(n);
			}
			if (sel)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Open ThemeZ Editor"))
	{
		ui->Window("ThemeZ").visible = true;
		ui->PersistVisibility("ThemeZ");
	}
}
