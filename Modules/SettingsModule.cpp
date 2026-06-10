#include "SettingsModule.h"
#include "ThemeZModule.h"

#include "../Core/UiConfig.h"
#include "../Core/Widgets.h"
#include "../Core/ThemeManager.h"
#include "../Core/ActorManager.h"
#include "../Core/PeerData.h"

#include "mq/imgui/ImGuiUtils.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
const char* kDisplayWindows[] = {
	"Player", "Pet", "Group", "Raid", "Buffs", "Songs", "Spells", "Casting", "XPBars", "AA", "HUD", "Inventory", "BigBag", "iTrack"
};

// Left-pane window list: visible windows plus Toolbar (which has scaling + an ItemSize option).
const char* kSettingsWindows[] = {
	"Player", "Pet", "Group", "Raid", "Buffs", "Songs", "Spells", "Casting", "XPBars", "AA", "HUD", "Inventory", "BigBag", "iTrack", "Toolbar"
};

const char* kHotkeyWindows[] = { "Inventory", "BigBag", "iTrack" };
const char* kModOptions[] = { "None", "Ctrl", "Alt", "Shift" };
const char* kMouseOptions[] = { "None", "Middle" };

bool IsHotkeyWindow(const std::string& name)
{
	for (const char* w : kHotkeyWindows)
	{
		if (ci_equals(name, w))
		{
			return true;
		}
	}
	return false;
}

struct NumMeta { float step; float min; float max; const char* fmt; };

bool NumMetaFor(const std::string& key, NumMeta& out)
{
	if (key == "NavDist")        { out = { 1.0f, 0.0f, 500.0f, "%.0f" }; return true; }
	if (key == "FlashThreshold") { out = { 0.5f, 0.0f, 300.0f, "%.0f" }; return true; }
	if (key == "GemHeight")      { out = { 1.0f, 8.0f, 128.0f, "%.0f" }; return true; }
	if (key == "ItemSize")       { out = { 1.0f, 16.0f, 128.0f, "%.0f" }; return true; }
	if (key == "MinSlotsWarn")   { out = { 1.0f, 0.0f, 100.0f, "%.0f" }; return true; }
	return false;
}

// A num that has its own dedicated widget elsewhere in the window detail.
bool NumHasCustomWidget(const std::string& window, const std::string& key)
{
	return ci_equals(window, "Raid") && key == "TileWidth";
}

// A str that is not a free-form string field (hotkeys + serialized colors are handled specially).
bool StrIsSpecial(const std::string& key)
{
	return key == "ToggleKey" || key == "ToggleMouse" || key == "ToggleMod1" || key == "ToggleMod2" || key == "ToggleMod3"
		|| key == "OutOfZoneColor";
}

bool ExcludedDisplayFlag(const std::string& win, const std::string& flag)
{
	if (win == "Inventory" && ci_starts_with(flag, "BagLock"))
	{
		return true;
	}
	if (win == "XPBars" && flag == "AutoSize")
	{
		return true;
	}
	return false;
}

std::vector<SettingRow> DiffRows(const std::vector<SettingRow>& before, const std::vector<SettingRow>& after)
{
	std::unordered_map<std::string, std::string> prev;
	prev.reserve(before.size());
	for (const SettingRow& r : before)
	{
		prev[r.module + "\x1f" + r.name] = r.value;
	}

	std::vector<SettingRow> delta;
	for (const SettingRow& r : after)
	{
		auto it = prev.find(r.module + "\x1f" + r.name);
		if (it == prev.end() || it->second != r.value)
		{
			delta.push_back(r);
		}
	}
	return delta;
}

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
	if (myui::StyledBeginCombo(label, current))
	{
		for (int i = 0; i < count; ++i)
		{
			bool selected = value == options[i];
			if (myui::PillSelectable(options[i], selected))
			{
				value = options[i];
				ImGui::CloseCurrentPopup();
			}
		}
		myui::StyledEndCombo();
	}
}

void DrawKeyCombo(const char* label, std::string& value)
{
	const char* current = value.empty() ? "None" : value.c_str();
	ImGui::SetNextItemWidth(90.0f);
	if (myui::StyledBeginCombo(label, current))
	{
		for (const std::string& option : KeyOptions())
		{
			bool isNone = option == "None";
			bool selected = isNone ? value.empty() : value == option;
			if (myui::PillSelectable(option.c_str(), selected))
			{
				value = isNone ? "" : option;
				ImGui::CloseCurrentPopup();
			}
		}
		myui::StyledEndCombo();
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

const char* WindowDescription(const std::string& name)
{
	if (name == "Player")      { return "Your health / mana / endurance bars, combat state, and (optionally) the target block."; }
	if (name == "Pet")         { return "Your pet's health and target, plus configurable command buttons (attack, guard, follow, ...)."; }
	if (name == "Group")       { return "Your group as a compact list - HP/mana/end bars, level, distance, roles, and pets."; }
	if (name == "Raid")        { return "A tiled grid of your raid, merging in-zone members with out-of-zone boxed characters."; }
	if (name == "Buffs")       { return "Your long-duration buffs (or another box's), as a list or an icon grid."; }
	if (name == "Songs")       { return "Your short-duration buffs / bard songs, as a list or an icon grid."; }
	if (name == "Spells")      { return "Your memorized spell gems - left-click to cast, right-click an empty gem to memorize."; }
	if (name == "Casting")     { return "A cast bar showing the spell being cast and the time remaining."; }
	if (name == "XPBars")      { return "Per-character XP / AA / breath tiles for your boxes, with AA allocation buttons."; }
	if (name == "AA")          { return "Browse, buy, and hotkey alternate-advancement abilities."; }
	if (name == "HUD")         { return "A small always-on status HUD (combat state, optional HP/Mana)."; }
	if (name == "Inventory")   { return "Your equipped paperdoll, bags, currency, and a character Stats tab."; }
	if (name == "BigBag")      { return "An all-in-one inventory browser - items, clickies, augments, bank, with search and sort."; }
	if (name == "iTrack")      { return "A shared cross-character item tracker - broadcast a list of items to watch for."; }
	return "";
}

const char* FlagDescription(const std::string& window, const std::string& flag)
{
	static const std::unordered_map<std::string, const char*> kDesc = {
		{ "ShowTarget",          "Show the target block (target HP, distance, and buffs) below your bars." },
		{ "SplitTarget",         "Move the target block into its own separate, movable window." },
		{ "CombatPulse",         "Pulse a red border around the player block while you're in combat." },
		{ "TargetTextOverlay",   "Draw the target's name and level over the target HP bar; raise the TargetHP bar height to grow it up behind the text." },
		{ "ShowMana",            "Show a mana bar for each group member." },
		{ "ShowEnd",             "Show an endurance bar for each group member." },
		{ "ShowPet",             "Show each member's pet HP bar." },
		{ "ShowSelf",            "Include yourself as a row in the group window." },
		{ "ShowRoleIcons",       "Show main-tank / main-assist / puller role icons and line-of-sight." },
		{ "ShowLevel",           "Show each member's level." },
		{ "ShowMoveStatus",      "Show a moving / sitting status indicator for each member." },
		{ "ShowDistance",        "Show each member's distance from you." },
		{ "ShowDirectionRing",   "Wrap the distance in a direction ring: a glowing marker orbits the ring to point at the spawn relative to your facing (up = ahead). Style it in the Direction Ring section below." },
		{ "VertPet",             "Draw the group pet bars vertically instead of horizontally." },
		{ "ShowEmptySlots",      "Draw an empty placeholder tile for each unfilled group slot." },
		{ "IconView",            "Show as an icon grid instead of a list with names and timers." },
		{ "ShowGroupOnly",       "Limit the character picker (for viewing another box's buffs) to your group members." },
		{ "ShowTimer",           "Show the remaining time on each entry." },
		{ "Vertical",            "Stack the spell gems vertically instead of in a horizontal row." },
		{ "ShowTooltip",         "Show a tooltip with exact XP / AA percentages on hover." },
		{ "AlphaSort",           "Sort the boxes alphabetically by name." },
		{ "MyGroupOnly",         "Only show tiles for characters in your group." },
		{ "ShowLeader",          "Show a leader icon on the group / raid leader's tile." },
		{ "TrainableOnly",       "Only list AAs you can currently train." },
		{ "AffordableOnly",      "Only list AAs you can currently afford." },
		{ "ConfirmPurchase",     "Ask for confirmation before buying an AA." },
		{ "ShowHotkeyButton",    "Show the Hotkey button (creates a hotkey for the selected AA)." },
		{ "ShowHpMana",          "Show HP / Mana readouts on the floating status HUD." },
		{ "ShowSlotBackground",  "Draw the slot background art behind item icons." },
		{ "AutoInventoryOnSwap", "Automatically stow the displaced item when you equip-swap a slot." },
		{ "UseableOnly",         "Show only gear/clickies you can use (class / race / deity) plus spell scrolls your class can learn; hides food, drink, other classes' spells, and non-equippable no-click clutter (tradeskill mats, runes, gems). Required level isn't filtered — too-high items stay, flagged with a lock icon; scribed scrolls get a check icon." },
		{ "SortName",            "Sort items by name." },
		{ "SortStack",           "Sort items by stack size." },
		{ "SortType",            "Sort items by item type." },
	};
	(void)window;
	auto it = kDesc.find(flag);
	return it != kDesc.end() ? it->second : "";
}
}

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
		if (m_ctx.Themes && m_themePreviewActive)
		{
			m_ctx.Themes->ClearPreview();
			m_themePreviewActive = false;
			m_themeLoaded = false;
		}
		m_wasVisible = false;
		m_remoteActive = false;
		return;
	}

	// Snapshot our own settings when the window opens, to compute the delta we broadcast on save.
	if (!m_wasVisible)
	{
		if (m_ctx.Settings)
		{
			m_localBaseline = m_ctx.Settings->GetAllSettings(m_ctx.Settings->Server(), m_ctx.Settings->Character());
		}
		m_wasVisible = true;
	}

	myui::SetGlobalToggleStyle(ComposeToggleFlags(local),
		ImVec2(local->Num("Toggles", "Width", 0.0f), local->Num("Toggles", "Height", 0.0f)));

	m_active = m_remoteActive ? &m_remote : local;
	m_drewGeneralThisFrame = false;

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

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - 560.0f) * 0.5f, vp->WorkPos.y + (vp->WorkSize.y - 600.0f) * 0.5f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 600.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &w.visible))
	{
		DrawTargetPicker();

		if (m_remoteActive)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
				"Editing %s (remote) - Save applies the changes to that character.", m_targetChar.c_str());
		}

		if (myui::StyledButton("Save Settings"))
		{
			if (m_remoteActive)
			{
				// Always write our local mirror so our copy stays consistent.
				m_remote.SaveForCharacter(m_ctx.Settings, m_targetServer, m_targetChar);

				std::vector<SettingRow> delta;
				if (m_ctx.Settings)
				{
					std::vector<SettingRow> after = m_ctx.Settings->GetAllSettings(m_targetServer, m_targetChar);
					delta = DiffRows(m_remoteBaseline, after);
					m_remoteBaseline = after;
				}

				if (m_ctx.Actors)
				{
					const auto& peers = m_ctx.Actors->Peers();
					auto it = peers.find(myui::PeerKey(m_targetServer, m_targetChar));
					if (it != peers.end())
					{
						if (m_ctx.Actors->IsSameStore(it->second))
						{
							// Shared DB: our mirror write already updated their store; nudge a reload.
							m_ctx.Actors->SendCommand(m_targetServer, m_targetChar, "ReloadSettings");
						}
						else
						{
							// Remote store: push the changed rows for them to apply authoritatively.
							m_ctx.Actors->SendSettingsPush(m_targetServer, m_targetChar, delta, false);
						}
					}
					// Offline target: nothing to send; the mirror write reconciles on their next join.
				}
			}
			else
			{
				local->Save();
				if (m_ctx.Settings)
				{
					std::vector<SettingRow> after = m_ctx.Settings->GetAllSettings(m_ctx.Settings->Server(), m_ctx.Settings->Character());
					std::vector<SettingRow> delta = DiffRows(m_localBaseline, after);
					m_localBaseline = after;
					if (m_ctx.Actors && !delta.empty())
					{
						m_ctx.Actors->BroadcastSettingsPush(delta);
					}
				}
			}
		}
		ImGui::SameLine();
		if (myui::StyledButton("Revert"))
		{
			if (m_remoteActive)
			{
				m_remote.LoadForCharacter(m_ctx.Settings, m_targetServer, m_targetChar);
				if (m_ctx.Settings)
				{
					m_remoteBaseline = m_ctx.Settings->GetAllSettings(m_targetServer, m_targetChar);
				}
			}
			else
			{
				local->RequestReload();
			}
		}
		ImGui::SameLine();
		if (myui::StyledButton("Copy Settings..."))
		{
			local->Window("Copy").visible = true;
			local->PersistVisibility("Copy");
		}

		ImGui::Separator();

		static const char* const kTabs[] = { "Settings", "Bars" };
		m_settingsTab = myui::PillTabBar("##SettingsTabs", kTabs, 2, m_settingsTab);
		ImGui::Spacing();
		if (m_settingsTab == 1)
		{
			DrawBarsTab();
		}
		else
		{
			DrawSettingsTab();
		}
	}
	ImGui::End();

	if (pushedColors > 0)
	{
		ImGui::PopStyleColor(pushedColors);
	}

	// Theme preview is only live while the General entry is showing the folded editor.
	if (m_ctx.Themes && !m_drewGeneralThisFrame && m_themePreviewActive)
	{
		m_ctx.Themes->ClearPreview();
		m_themePreviewActive = false;
		m_themeLoaded = false;
	}

	if (!w.visible)
	{
		local->PersistVisibility(GetName());
		local->Window("Copy").visible = false;
		local->previewCasting = false;
		m_remoteActive = false;
		m_wasVisible = false;
		if (m_ctx.Themes && m_themePreviewActive)
		{
			m_ctx.Themes->ClearPreview();
			m_themePreviewActive = false;
			m_themeLoaded = false;
		}
	}
}

void SettingsModule::DrawTargetPicker()
{
	ImGui::TextDisabled("Edit this character, or push changes to another box.");
	ImGui::SameLine();
	mq::imgui::HelpMarker("Online characters apply your changes live and confirm them back. Offline characters are "
		"written only to your local mirror DB and sync the next time they come online.");

	std::string current = m_remoteActive ? (m_targetServer + " / " + m_targetChar) : std::string("This Character");
	ImGui::SetNextItemWidth(220.0f);
	if (myui::StyledBeginCombo("Editing##tgt", current.c_str()))
	{
		if (myui::PillSelectable("This Character", !m_remoteActive))
		{
			m_remoteActive = false;
			ImGui::CloseCurrentPopup();
		}

		auto selectTarget = [&](const std::string& server, const std::string& chr) {
			m_targetServer = server;
			m_targetChar = chr;
			m_remoteActive = true;
			m_remote.LoadForCharacter(m_ctx.Settings, server, chr);
			if (m_ctx.Settings)
			{
				m_remoteBaseline = m_ctx.Settings->GetAllSettings(server, chr);
			}
			ImGui::CloseCurrentPopup();
		};

		std::unordered_set<std::string> seen;
		if (m_ctx.Actors)
		{
			for (const auto& [key, peer] : m_ctx.Actors->Peers())
			{
				seen.insert(myui::PeerKey(peer.server, peer.character));
				bool sel = m_remoteActive && ci_equals(peer.character, m_targetChar) && ci_equals(peer.server, m_targetServer);
				std::string label = peer.server + " / " + peer.character + " (online)";
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.9f, 0.45f, 1.0f));
				bool clicked = myui::PillSelectable(label.c_str(), sel);
				ImGui::PopStyleColor();
				if (clicked)
				{
					selectTarget(peer.server, peer.character);
				}
			}
		}

		if (m_ctx.Settings)
		{
			for (const CharIdent& c : m_ctx.Settings->GetCharacterList())
			{
				if (ci_equals(c.character, m_ctx.Settings->Character()) && ci_equals(c.server, m_ctx.Settings->Server()))
				{
					continue;
				}
				if (seen.count(myui::PeerKey(c.server, c.character)))
				{
					continue;
				}
				bool sel = m_remoteActive && ci_equals(c.character, m_targetChar) && ci_equals(c.server, m_targetServer);
				std::string label = c.server + " / " + c.character + " (offline)";
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				bool clicked = myui::PillSelectable(label.c_str(), sel);
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Offline - saving writes only your local mirror DB; it syncs when this character next comes online.");
				}
				if (clicked)
				{
					selectTarget(c.server, c.character);
				}
			}
		}

		myui::StyledEndCombo();
	}
}

void SettingsModule::DrawSettingsTab()
{
	ImGui::BeginChild("##WinList", ImVec2(150.0f, 0.0f), ImGuiChildFlags_Borders);
	if (myui::PillSelectable("General", m_selWindow.empty()))
	{
		m_selWindow.clear();
	}
	ImGui::Separator();
	for (const char* name : kSettingsWindows)
	{
		if (myui::PillSelectable(name, ci_equals(m_selWindow, name)))
		{
			m_selWindow = name;
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("##WinDetail", ImVec2(0.0f, 0.0f));
	if (m_selWindow.empty())
	{
		DrawGeneralDetail();
	}
	else
	{
		DrawWindowDetail(m_selWindow);
	}
	ImGui::EndChild();
}

void SettingsModule::DrawGeneralDetail()
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;

	ImGui::SeparatorText("Animations");
	{
		bool anim = ui->Flag("Global", "AnimatedWidgets", true);
		if (myui::DrawToggle("Animated Widgets##tgAnimGlobal", &anim))
		{
			ui->SetFlag("Global", "AnimatedWidgets", anim);
		}
		ImGui::TextWrapped("Master switch for the animated styled-widget effects: button glow/press, toggle glow/pulse/rock, and toolbar hover. Off keeps the theme colors but draws the resting state with no motion.");
	}

	ImGui::SeparatorText("Toggle Switches");
	ImGui::TextWrapped("Configure the look of every toggle switch in the UI.");

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

	float width = ui->Num("Toggles", "Width", 0.0f);
	float height = ui->Num("Toggles", "Height", 0.0f);
	ImGui::SetNextItemWidth(160.0f);
	if (myui::StyledSliderFloat("Width (0 = auto)##tgw", &width, 0.0f, 80.0f, "%.0f"))
	{
		ui->SetNum("Toggles", "Width", width);
	}
	ImGui::SetNextItemWidth(160.0f);
	if (myui::StyledSliderFloat("Height (0 = auto)##tgh", &height, 0.0f, 60.0f, "%.0f"))
	{
		ui->SetNum("Toggles", "Height", height);
	}

	ImGui::TextUnformatted("Preview:");
	ImGui::SameLine();
	static bool s_previewOn = true;
	myui::DrawToggle("Sample##tgPreview", &s_previewOn);

	// Theme editing is a local/global concept; hide it while editing a remote character.
	ThemeManager* themes = m_ctx.Themes;
	if (m_remoteActive || !themes)
	{
		return;
	}

	ImGui::SeparatorText("Theme");
	ImGui::SetNextItemWidth(110.0f);
	if (myui::StyledBeginCombo("Active Theme", themes->ActiveName().c_str()))
	{
		for (const std::string& n : themes->GetThemeNames())
		{
			bool sel = (n == themes->ActiveName());
			if (myui::PillSelectable(n.c_str(), sel))
			{
				themes->SetActiveTheme(n);
				ImGui::CloseCurrentPopup();
			}
			if (sel)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		myui::StyledEndCombo();
	}
	ImGui::SameLine();
	if (myui::StyledButton("Open Themes Editor"))
	{
		ui->Window("ThemeEditor").visible = true;
		ui->PersistVisibility("ThemeEditor");
	}
	// The full theme editor (Edit Theme / Name / Colors / Styles) lives in the
	// Themes editor window; the General tab only picks the active theme.
}

void SettingsModule::DrawWindowDetail(const std::string& name)
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;
	WindowConfig& w = ui->Window(name);

	const bool hasShownLocked = !ci_equals(name, "Toolbar");
	const bool isPet = ci_equals(name, "Pet");
	const bool isRaid = ci_equals(name, "Raid");

	ImGui::SeparatorText("General");
	if (const char* d = WindowDescription(name); d && *d)
	{
		ImGui::TextWrapped("%s", d);
	}

	if (ImGui::BeginTable("##gen", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
	{
		auto labelCol = [](const char* label) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
		};

		if (hasShownLocked)
		{
			labelCol("Shown");
			if (myui::DrawToggle((std::string("##vis") + name).c_str(), &w.visible) && !m_remoteActive)
			{
				ui->PersistVisibility(name);
			}
			labelCol("Locked");
			if (myui::DrawToggle((std::string("##lock") + name).c_str(), &w.locked) && !m_remoteActive)
			{
				ui->PersistField(name, "Locked");
			}
		}

		labelCol("Text Scale");
		ImGui::SetNextItemWidth(-FLT_MIN);
		myui::StyledSliderFloat((std::string("##ts") + name).c_str(), &w.textScale, 0.3f, 3.0f, "%.2f");
		labelCol("Icon Scale");
		ImGui::SetNextItemWidth(-FLT_MIN);
		myui::StyledSliderFloat((std::string("##is") + name).c_str(), &w.iconScale, 0.3f, 3.0f, "%.2f");

		ImGui::EndTable();
	}

	// Display flags + per-window specials.
	bool hasDisplay = !w.flags.empty() || isRaid || ci_equals(name, "XPBars") || ci_equals(name, "Group");
	if (hasDisplay)
	{
		ImGui::SeparatorText("Display");
		if (isPet)
		{
			ImGui::TextDisabled("Buttons shown");
			ImGui::SameLine();
			mq::imgui::HelpMarker("Toggle which pet command buttons appear on the Pet window (Attack, Guard, Follow, ...).");
		}

		if (!w.flags.empty())
		{
			float avail = ImGui::GetContentRegionAvail().x;
			int cols = (int)(avail / 175.0f);
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
					if (ExcludedDisplayFlag(name, flag))
					{
						continue;
					}
					ImGui::TableNextColumn();
					myui::DrawToggle((flag + "##" + name).c_str(), &val);
					if (!isPet)
					{
						if (const char* d = FlagDescription(name, flag); d && *d)
						{
							ImGui::SameLine(0.0f, 4.0f);
							mq::imgui::HelpMarker(d);
						}
					}
				}
				ImGui::EndTable();
			}
		}

		if (ci_equals(name, "XPBars"))
		{
			if (myui::StyledButton("Fit to Tiles##XPBarsFit"))
			{
				m_ctx.UI->SetFlag("XPBars", "AutoSize", true);
			}
			ImGui::SameLine();
			mq::imgui::HelpMarker("Resize the XP Bars window once to clip tightly around the current tile layout, then leave it freely sizable (unless the window is locked). Press after adding/removing characters or changing the window width.");
		}

		if (ci_equals(name, "Group"))
		{
			MQColor ooz = ui->Color("Group", "OutOfZoneColor", MQColor(116, 116, 116, 255));
			if (ColorEditMQ("Out-of-Zone Bar Color##GroupOOZ", ooz))
			{
				ui->SetColor("Group", "OutOfZoneColor", ooz);
			}
			ImGui::SameLine();
			mq::imgui::HelpMarker("Color used for all bars (HP/Mana/End/Pet) of out-of-zone members in both the Group and Raid windows.");
		}

		if (isRaid)
		{
			float tileWidth = ui->Num("Raid", "TileWidth", 180.0f);
			ImGui::SetNextItemWidth(160.0f);
			if (myui::StyledSliderFloat("Tile Width##RaidTW", &tileWidth, 80.0f, 600.0f, "%.0f"))
			{
				ui->SetNum("Raid", "TileWidth", tileWidth);
			}
			ImGui::SameLine();
			mq::imgui::HelpMarker("Width of each raid member tile, in pixels.");
		}
	}

	if (ci_equals(name, "Player") || ci_equals(name, "Group") || ci_equals(name, "Raid"))
	{
		DrawRingEditor(name);
	}

	// Generic numeric / string options not handled above.
	bool hasOptions = false;
	for (const auto& [key, val] : w.nums)
	{
		(void)val;
		if (!NumHasCustomWidget(name, key))
		{
			hasOptions = true;
			break;
		}
	}
	if (!hasOptions)
	{
		for (const auto& [key, val] : w.strs)
		{
			(void)val;
			if (!StrIsSpecial(key))
			{
				hasOptions = true;
				break;
			}
		}
	}

	if (hasOptions)
	{
		ImGui::SeparatorText("Options");
		if (ImGui::BeginTable("##opt", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
		{
			auto labelCol = [](const std::string& label) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label.c_str());
				ImGui::TableNextColumn();
			};

			for (auto& [key, val] : w.nums)
			{
				if (NumHasCustomWidget(name, key))
				{
					continue;
				}
				labelCol(key);
				NumMeta meta;
				if (!NumMetaFor(key, meta))
				{
					meta = { 0.5f, 0.0f, 0.0f, "%.2f" };
				}
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (meta.max > meta.min)
				{
					myui::StyledSliderFloat((std::string("##num") + name + key).c_str(), &val, meta.min, meta.max, meta.fmt);
				}
				else
				{
					ImGui::DragFloat((std::string("##num") + name + key).c_str(), &val, meta.step, meta.min, meta.max, meta.fmt);
				}
			}

			for (auto& [key, val] : w.strs)
			{
				if (StrIsSpecial(key))
				{
					continue;
				}
				labelCol(key);
				ImGui::SetNextItemWidth(-FLT_MIN);
				myui::StyledEditField((std::string("##str") + name + key).c_str(), &val);
			}

			ImGui::EndTable();
		}
	}

	// Hotkeys (Inventory / BigBag / iTrack).
	if (IsHotkeyWindow(name))
	{
		ImGui::SeparatorText("Hotkeys");
		ImGui::TextDisabled("Toggle this window with up to 3 modifiers plus a key, and/or the middle mouse button.");
		std::string id = name;
		DrawKeyCombo((std::string("Key##k") + id).c_str(), w.strs["ToggleKey"]);
		ImGui::SameLine();
		OptionCombo((std::string("Mouse##mo") + id).c_str(), w.strs["ToggleMouse"], kMouseOptions, 2);
		OptionCombo((std::string("Mod 1##m1") + id).c_str(), w.strs["ToggleMod1"], kModOptions, 4);
		ImGui::SameLine();
		OptionCombo((std::string("Mod 2##m2") + id).c_str(), w.strs["ToggleMod2"], kModOptions, 4);
		ImGui::SameLine();
		OptionCombo((std::string("Mod 3##m3") + id).c_str(), w.strs["ToggleMod3"], kModOptions, 4);
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
		bool open = (m_openBarTree == name);
		const bool wasOpen = open;
		const bool visible = myui::BeginAnimatedTreeNode(name, &open);
		if (open != wasOpen)
		{
			// Exclusive accordion: opening a branch collapses the others.
			m_openBarTree = open ? name : std::string();
		}
		if (visible)
		{
			for (auto& [role, style] : w.bars)
			{
				bool selected = (m_barWindow == name && m_barRole == role);
				if (myui::PillSelectable((role + "##" + name).c_str(), selected))
				{
					m_barWindow = name;
					m_barRole = role;
				}
			}
			myui::EndAnimatedTreeNode();
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

	auto tip = [](const char* t) { ImGui::SameLine(); mq::imgui::HelpMarker(t); };

	ImGui::Text("%s / %s", window.c_str(), role.c_str());
	ImGui::SameLine();
	if (myui::StyledSmallButton("Reset Effects"))
	{
		b.gradientOn = false;
		b.border = false;
		b.ticksOn = false;
		b.shimmerOn = false;
		b.glowOn = false;
		b.tweenSeconds = 0.0f;
	}

	if (myui::StyledButton("Copy this style to..."))
	{
		ImGui::OpenPopup("##CopyBarTo");
	}
	// PillSelectable fills the available width, so give the auto-sizing popup a
	// fixed width (otherwise it collapses to a sliver with no room for the pills).
	ImGui::SetNextWindowSizeConstraints(ImVec2(240.0f, 0.0f), ImVec2(240.0f, FLT_MAX));
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
				if (myui::PillSelectable((std::string(w2) + " / " + r2).c_str(), false))
				{
					s2 = b;
					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::EndPopup();
	}

	if (window == "Casting" && role == "Cast")
	{
		ImGui::SameLine();
		myui::StyledCheckbox("Force Show", &ui->previewCasting);
	}

	ImGui::Separator();

	const bool groupPet = ((window == "Group" || window == "Raid") && role == "Pet");

	if (myui::BeginAnimatedHeader("Geometry", true))
	{
		if (!groupPet)
		{
			b.vertical = false;
		}
		myui::StyledSliderFloat("Height", &b.height, 1.0f, 200.0f, "%.0f");
		if (groupPet)
		{
			myui::StyledSliderFloat("Width (vertical)", &b.width, 1.0f, 200.0f, "%.0f");
		}
		myui::StyledSliderFloat("Rounding", &b.rounding, 0.0f, 24.0f, "%.1f");
		myui::StyledSliderFloat("Pad End", &b.padEnd, 0.0f, 40.0f, "%.0f");
		myui::EndAnimatedHeader();
	}

	if (myui::BeginAnimatedHeader("Fill", true))
	{
		ColorEditMQ("Fill Low", b.fillLow);
		tip("Fill color toward 0% (and the gradient's start color).");
		ColorEditMQ("Fill High", b.fillHigh);
		tip("Fill color toward 100% (and the gradient's end color).");
		ColorEditMQ("Background", b.bgColor);
		tip("Color of the empty (unfilled) part of the bar.");
		myui::StyledCheckbox("Gradient", &b.gradientOn);
		tip("Blend Fill Low -> Fill High across the fill instead of a solid color.");
		if (b.gradientOn)
		{
			const char* modes[] = { "Static", "Dynamic" };
			myui::StyledCombo("Gradient Mode", &b.gradientMode, modes, 2);
			tip("Static: a fixed Low->High gradient. Dynamic: the end color tracks the current fill %.");
			const char* dirs[] = { "Horizontal", "Vertical", "Diag TL-BR", "Diag TR-BL" };
			myui::StyledCombo("Gradient Dir", &b.gradientDir, dirs, 4);
			tip("Direction the gradient runs across the bar.");
		}
		myui::EndAnimatedHeader();
	}

	if (myui::BeginAnimatedHeader("Text", false))
	{
		const char* modes[] = { "None", "Percent", "Value" };
		myui::StyledCombo("Text Mode", &b.textMode, modes, 3);
		if (b.textMode != 0)
		{
			if (b.textMode == 1)
			{
				myui::StyledEditField("Format", &b.textFormat);
			}
			myui::StyledSliderFloat("Text Scale", &b.textScale, 0.3f, 3.0f, "%.2f");
			myui::StyledCheckbox("Drop Shadow", &b.textDropShadow);
		}
		myui::EndAnimatedHeader();
	}

	if (myui::BeginAnimatedHeader("Border", false))
	{
		myui::StyledCheckbox("Enable Border", &b.border);
		if (b.border)
		{
			myui::StyledSliderFloat("Border Thickness", &b.borderThickness, 0.5f, 6.0f, "%.1f");
			ColorEditMQ("Border Color", b.borderColor);
		}
		myui::EndAnimatedHeader();
	}

	if (myui::BeginAnimatedHeader("Efx", false))
	{
		myui::StyledCheckbox("Ticks", &b.ticksOn);
		tip("Draw evenly-spaced marker lines across the bar.");
		if (b.ticksOn)
		{
			ImGui::Indent();
			myui::StyledSliderFloat("Tick Every (0-1)", &b.tickEvery, 0.01f, 1.0f, "%.2f");
			tip("Spacing between ticks as a fraction of the bar (0.10 = every 10%).");
			myui::StyledSliderFloat("Tick Thickness", &b.tickThickness, 0.5f, 4.0f, "%.1f");
			myui::StyledSliderInt("Tick Alpha", &b.tickAlpha, 0, 255);
			ImGui::Unindent();
		}

		myui::StyledCheckbox("Shimmer", &b.shimmerOn);
		tip("A bright highlight that sweeps along the filled portion.");
		if (b.shimmerOn)
		{
			ImGui::Indent();
			myui::StyledSliderFloat("Shimmer Speed", &b.shimmerSpeed, 0.0f, 4.0f, "%.2f");
			myui::StyledSliderFloat("Shimmer Width", &b.shimmerWidth, 4.0f, 120.0f, "%.0f");
			myui::StyledCheckbox("Follows Progress", &b.shimmerFollows);
			tip("Reverse the shimmer's sweep direction while the bar is draining.");
			ImGui::Unindent();
		}

		myui::StyledCheckbox("Glow", &b.glowOn);
		tip("A soft glow at the leading edge of the fill.");
		if (b.glowOn)
		{
			ImGui::Indent();
			myui::StyledSliderFloat("Glow Thickness", &b.glowThickness, 0.0f, 60.0f, "%.0f");
			myui::StyledSliderInt("Glow Alpha", &b.glowAlpha, 0, 255);
			ImGui::Unindent();
		}

		ImGui::SeparatorText("Animation");
		myui::StyledSliderFloat("Tween Seconds", &b.tweenSeconds, 0.0f, 2.0f, "%.2f");
		tip("Animate the bar smoothly toward new values over this many seconds (0 = snap instantly).");
		myui::EndAnimatedHeader();
	}

	ImGui::PopID();
	ImGui::PopID();
}

void SettingsModule::DrawRingEditor(const std::string& window)
{
	UiConfig* ui = m_active ? m_active : m_ctx.UI;
	RingStyle& r = ui->Ring(window, "Direction");

	if (!myui::BeginAnimatedHeader("Direction Ring", false))
	{
		return;
	}

	ImGui::PushID(window.c_str());
	ImGui::PushID("ring");

	auto tip = [](const char* t) { ImGui::SameLine(); mq::imgui::HelpMarker(t); };

	static const char* const kModes[] = { "Default", "Distance", "Visibility" };
	r.trackMode = myui::PillTabBar("##ringmode", kModes, 3, r.trackMode);
	tip("Ring track color. Default = theme frame background; Distance = tween near->far color over a range; Visibility = color by line of sight.");

	if (r.trackMode == 1)
	{
		ColorEditMQ("Near Color", r.distNear);
		ColorEditMQ("Far Color", r.distFar);
		myui::StyledSliderFloat("Near Distance", &r.distMin, 0.0f, 500.0f, "%.0f");
		myui::StyledSliderFloat("Far Distance", &r.distMax, 0.0f, 500.0f, "%.0f");
		tip("Track tweens from Near to Far color as the spawn's distance crosses this range.");
	}
	else if (r.trackMode == 2)
	{
		ColorEditMQ("In-Sight Color", r.losColor);
		ColorEditMQ("Blocked Color", r.noLosColor);
	}
	else
	{
		ColorEditMQ("Ring Color", r.ringColor);
		tip("Track color. Set alpha to 0 to follow the theme frame background.");
	}

	ImGui::SeparatorText("Indicator");
	ColorEditMQ("Indicator Color", r.indicColor);
	tip("Marker color. Set alpha to 0 to follow the theme slider-grab color.");
	myui::StyledCheckbox("Glow", &r.glowOn);
	if (r.glowOn)
	{
		ColorEditMQ("Glow Color", r.glowColor);
		tip("Set alpha to 0 to glow with the indicator color.");
		myui::StyledSliderFloat("Glow Strength", &r.glowAlpha, 0.0f, 1.0f, "%.2f");
	}

	ImGui::SeparatorText("Size");
	myui::StyledSliderFloat("Thickness", &r.thickness, 1.0f, 10.0f, "%.1f");
	myui::StyledSliderFloat("Indicator Size", &r.indicSize, 2.0f, 14.0f, "%.1f");

	ImGui::PopID();
	ImGui::PopID();
	myui::EndAnimatedHeader();
}

