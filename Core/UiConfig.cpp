#include "UiConfig.h"
#include "SettingsStore.h"
#include "InventoryData.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <set>

namespace
{
std::string ColorToStr(const MQColor& c)
{
	return fmt::format("{{{},{},{},{}}}", (int)c.Red, (int)c.Green, (int)c.Blue, (int)c.Alpha);
}

MQColor StrToColor(const std::string& s, const MQColor& def)
{
	int v[4] = { 0, 0, 0, 255 };
	int n = 0;
	const char* p = s.c_str();
	const char* end = p + s.size();
	while (p < end && n < 4)
	{
		if (std::isdigit(static_cast<unsigned char>(*p)) || *p == '-')
		{
			char* next = nullptr;
			long val = std::strtol(p, &next, 10);
			if (next == p)
			{
				++p;
				continue;
			}
			v[n++] = (int)val;
			p = next;
		}
		else
		{
			++p;
		}
	}
	if (n < 3)
	{
		return def;
	}
	return MQColor((uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3]);
}

template <class V>
void VisitBar(BarStyle& s, V& v)
{
	v.bo("vertical", s.vertical);
	v.fl("height", s.height);
	v.fl("width", s.width);
	v.fl("padEnd", s.padEnd);
	v.fl("rounding", s.rounding);
	v.co("fillLow", s.fillLow);
	v.co("fillHigh", s.fillHigh);
	v.co("bgColor", s.bgColor);
	v.bo("gradientOn", s.gradientOn);
	v.in("gradientMode", s.gradientMode);
	v.in("gradientDir", s.gradientDir);
	v.bo("border", s.border);
	v.fl("borderThickness", s.borderThickness);
	v.co("borderColor", s.borderColor);
	v.in("textMode", s.textMode);
	v.st("textFormat", s.textFormat);
	v.fl("textScale", s.textScale);
	v.bo("textDropShadow", s.textDropShadow);
	v.bo("ticksOn", s.ticksOn);
	v.fl("tickEvery", s.tickEvery);
	v.fl("tickThickness", s.tickThickness);
	v.in("tickAlpha", s.tickAlpha);
	v.bo("shimmerOn", s.shimmerOn);
	v.fl("shimmerSpeed", s.shimmerSpeed);
	v.fl("shimmerWidth", s.shimmerWidth);
	v.bo("shimmerFollows", s.shimmerFollows);
	v.bo("glowOn", s.glowOn);
	v.fl("glowThickness", s.glowThickness);
	v.in("glowAlpha", s.glowAlpha);
	v.fl("tweenSeconds", s.tweenSeconds);
}

template <class V>
void VisitRing(RingStyle& s, V& v)
{
	v.in("trackMode", s.trackMode);
	v.co("ringColor", s.ringColor);
	v.co("indicColor", s.indicColor);
	v.fl("distMin", s.distMin);
	v.fl("distMax", s.distMax);
	v.co("distNear", s.distNear);
	v.co("distFar", s.distFar);
	v.co("losColor", s.losColor);
	v.co("noLosColor", s.noLosColor);
	v.bo("glowOn", s.glowOn);
	v.co("glowColor", s.glowColor);
	v.fl("glowAlpha", s.glowAlpha);
	v.fl("radius", s.radius);
	v.fl("thickness", s.thickness);
	v.fl("indicSize", s.indicSize);
}

struct SaveVisitor
{
	SettingsStore* store;
	std::string    window;
	std::string    prefix;

	void fl(const char* n, float& v)       { store->SetNumber(window, prefix + n, v); }
	void bo(const char* n, bool& v)        { store->SetBool(window, prefix + n, v); }
	void in(const char* n, int& v)         { store->SetNumber(window, prefix + n, (float)v); }
	void co(const char* n, MQColor& v)     { store->SetString(window, prefix + n, ColorToStr(v)); }
	void st(const char* n, std::string& v) { store->SetString(window, prefix + n, v); }
};

struct LoadVisitor
{
	SettingsStore* store;
	std::string    window;
	std::string    prefix;

	void fl(const char* n, float& v)       { v = store->GetNumber(window, prefix + n, v); }
	void bo(const char* n, bool& v)        { v = store->GetBool(window, prefix + n, v); }
	void in(const char* n, int& v)         { v = (int)store->GetNumber(window, prefix + n, (float)v); }
	void co(const char* n, MQColor& v)     { v = StrToColor(store->GetString(window, prefix + n, ColorToStr(v)), v); }
	void st(const char* n, std::string& v) { v = store->GetString(window, prefix + n, v); }
};

struct KeyCollector
{
	std::set<std::string>* out;
	void fl(const char* n, float&)       { out->insert(n); }
	void bo(const char* n, bool&)        { out->insert(n); }
	void in(const char* n, int&)         { out->insert(n); }
	void co(const char* n, MQColor&)     { out->insert(n); }
	void st(const char* n, std::string&) { out->insert(n); }
};

bool IsTransientVisibility(const std::string& window)
{
	return window == "BigBag" || window == "Inventory" || window == "iTrack" || window == "Settings" || window == "AA";
}

bool IsValidBarKey(const WindowConfig& w, const std::set<std::string>& barFields, const std::string& name)
{
	if (name.rfind("Bar.", 0) != 0)
	{
		return false;
	}
	size_t secondDot = name.find('.', 4);
	if (secondDot == std::string::npos)
	{
		return false;
	}
	std::string role = name.substr(4, secondDot - 4);
	std::string field = name.substr(secondDot + 1);
	return w.bars.count(role) != 0 && barFields.count(field) != 0;
}

bool IsValidRingKey(const WindowConfig& w, const std::set<std::string>& ringFields, const std::string& name)
{
	if (name.rfind("Ring.", 0) != 0)
	{
		return false;
	}
	size_t secondDot = name.find('.', 5);
	if (secondDot == std::string::npos)
	{
		return false;
	}
	std::string role = name.substr(5, secondDot - 5);
	std::string field = name.substr(secondDot + 1);
	return w.rings.count(role) != 0 && ringFields.count(field) != 0;
}
} // namespace

void UiConfig::EnsureWindow(const std::string& name)
{
	if (m_windows.find(name) == m_windows.end())
	{
		m_windows[name];
		m_order.push_back(name);
	}
}

WindowConfig& UiConfig::Window(const std::string& name)
{
	EnsureWindow(name);
	return m_windows[name];
}

BarStyle& UiConfig::Bar(const std::string& window, const std::string& role)
{
	WindowConfig& w = Window(window);
	return w.bars[role];
}

RingStyle& UiConfig::Ring(const std::string& window, const std::string& role)
{
	WindowConfig& w = Window(window);
	return w.rings[role];
}

bool UiConfig::Flag(const std::string& window, const std::string& name, bool def)
{
	WindowConfig& w = Window(window);
	auto it = w.flags.find(name);
	if (it != w.flags.end())
	{
		return it->second;
	}
	w.flags[name] = def;
	return def;
}

void UiConfig::SetFlag(const std::string& window, const std::string& name, bool value)
{
	Window(window).flags[name] = value;
}

float UiConfig::Num(const std::string& window, const std::string& name, float def)
{
	WindowConfig& w = Window(window);
	auto it = w.nums.find(name);
	if (it != w.nums.end())
	{
		return it->second;
	}
	w.nums[name] = def;
	return def;
}

void UiConfig::SetNum(const std::string& window, const std::string& name, float value)
{
	Window(window).nums[name] = value;
}

std::string UiConfig::Str(const std::string& window, const std::string& name, const std::string& def)
{
	WindowConfig& w = Window(window);
	auto it = w.strs.find(name);
	if (it != w.strs.end())
	{
		return it->second;
	}
	w.strs[name] = def;
	return def;
}

void UiConfig::SetStr(const std::string& window, const std::string& name, const std::string& value)
{
	Window(window).strs[name] = value;
}

MQColor UiConfig::Color(const std::string& window, const std::string& name, const MQColor& def)
{
	return StrToColor(Str(window, name, ColorToStr(def)), def);
}

void UiConfig::SetColor(const std::string& window, const std::string& name, const MQColor& value)
{
	SetStr(window, name, ColorToStr(value));
}

bool UiConfig::ToggleVisible(const std::string& window)
{
	WindowConfig& w = Window(window);
	w.visible = !w.visible;
	PersistVisibility(window);
	return w.visible;
}

void UiConfig::SeedDefaults()
{
	m_windows.clear();
	m_order.clear();

	auto bar = [&](const std::string& win, const std::string& role, MQColor lo, MQColor hi, float h) -> BarStyle& {
		BarStyle& b = Bar(win, role);
		b.fillLow = lo;
		b.fillHigh = hi;
		b.height = h;
		return b;
	};

	Window("Player");
	bar("Player", "HP", MQColor(190, 75, 75), MQColor(190, 75, 75), 18.0f);
	bar("Player", "Mana", MQColor(0, 0, 120), MQColor(40, 120, 255), 18.0f);
	bar("Player", "End", MQColor(120, 120, 0), MQColor(230, 230, 0), 18.0f);
	bar("Player", "TargetHP", MQColor(190, 75, 75), MQColor(190, 75, 75), 18.0f);
	bar("Player", "Aggro", MQColor(0, 160, 0), MQColor(200, 0, 0), 14.0f);
	SetFlag("Player", "ShowTarget", true);
	SetFlag("Player", "SplitTarget", false);
	SetFlag("Player", "CombatPulse", true);
	SetFlag("Player", "TargetTextOverlay", false);
	Ring("Player", "Direction");
	SetFlag("Player", "ShowDirectionRing", false);

	Window("Pet");
	bar("Pet", "HP", MQColor(190, 75, 75), MQColor(190, 75, 75), 18.0f);
	bar("Pet", "PetTarget", MQColor(190, 75, 75), MQColor(190, 75, 75), 18.0f);
	{
		const char* petButtons[] = { "Attack", "Back", "Guard", "Follow", "Sit", "Stop",
			"Taunt", "Hold", "NoHold", "Kill", "Health" };
		for (const char* btn : petButtons)
		{
			SetFlag("Pet", btn, true);
		}
	}

	Window("Group");
	bar("Group", "HP", MQColor(190, 75, 75), MQColor(190, 75, 75), 16.0f);
	bar("Group", "Mana", MQColor(66, 18, 128), MQColor(32, 151, 235), 16.0f);
	bar("Group", "End", MQColor(16, 99, 30), MQColor(210, 185, 1), 16.0f);
	{
		BarStyle& pb = bar("Group", "Pet", MQColor(210, 185, 1), MQColor(0, 160, 0), 16.0f);
		pb.vertical = true;
		pb.width = 16.0f;
	}
	SetFlag("Group", "ShowMana", true);
	SetFlag("Group", "ShowEnd", true);
	SetFlag("Group", "ShowPet", true);
	SetFlag("Group", "ShowSelf", false);
	SetFlag("Group", "ShowRoleIcons", true);
	SetFlag("Group", "ShowLevel", true);
	SetFlag("Group", "ShowMoveStatus", true);
	SetFlag("Group", "ShowDistance", true);
	SetFlag("Group", "VertPet", true);
	SetFlag("Group", "ShowEmptySlots", false);
	Ring("Group", "Direction");
	SetFlag("Group", "ShowDirectionRing", false);
	SetNum("Group", "NavDist", 10.0f);
	SetNum("Group", "MaxRaidColumns", 4.0f);
	SetColor("Group", "OutOfZoneColor", MQColor(116, 116, 116, 255));

	Window("Raid");
	bar("Raid", "HP", MQColor(190, 75, 75), MQColor(190, 75, 75), 10.0f);
	bar("Raid", "Mana", MQColor(66, 18, 128), MQColor(32, 151, 235), 10.0f);
	bar("Raid", "End", MQColor(16, 99, 30), MQColor(210, 185, 1), 10.0f);
	{
		BarStyle& pb = bar("Raid", "Pet", MQColor(210, 185, 1), MQColor(0, 160, 0), 10.0f);
		pb.vertical = true;
		pb.width = 10.0f;
	}
	Ring("Raid", "Direction");
	SetFlag("Raid", "ShowDirectionRing", false);
	SetNum("Raid", "TileWidth", 180.0f);

	Window("Buffs");
	SetFlag("Buffs", "IconView", false);
	SetFlag("Buffs", "ShowGroupOnly", true);
	SetNum("Buffs", "FlashThreshold", 18.0f);

	Window("Songs");
	SetFlag("Songs", "ShowTimer", true);
	SetFlag("Songs", "IconView", false);
	SetNum("Songs", "FlashThreshold", 18.0f);

	Window("Spells");
	SetFlag("Spells", "Vertical", false);
	SetNum("Spells", "GemHeight", 32.0f);

	Window("Casting");
	bar("Casting", "Cast", MQColor(160, 90, 0), MQColor(0, 200, 200), 18.0f);

	Window("XPBars");
	SetFlag("XPBars", "AutoSize", false);
	SetFlag("XPBars", "ShowTooltip", true);
	SetFlag("XPBars", "AlphaSort", false);
	SetFlag("XPBars", "MyGroupOnly", true);
	SetFlag("XPBars", "ShowLeader", false);
	bar("XPBars", "XP", MQColor(215, 180, 0), MQColor(255, 230, 100), 6.0f);
	bar("XPBars", "AAExp", MQColor(60, 60, 140), MQColor(120, 120, 255), 6.0f);
	bar("XPBars", "Air", MQColor(160, 90, 20), MQColor(240, 140, 25), 6.0f);

	Window("AA");
	SetFlag("AA", "TrainableOnly", false);
	SetFlag("AA", "AffordableOnly", false);
	SetFlag("AA", "ConfirmPurchase", false);
	SetFlag("AA", "ShowHotkeyButton", true);

	Window("HUD").visible = true;
	SetFlag("HUD", "ShowHpMana", false);

	Window("Toolbar").visible = true;
	SetNum("Toolbar", "ItemSize", 32.0f);

	Window("Inventory");
	SetFlag("Inventory", "ShowSlotBackground", true);
	SetFlag("Inventory", "AutoInventoryOnSwap", true);
	SetNum("Inventory", "ItemSize", 40.0f);
	{
		BarStyle& xp = bar("Inventory", "XP", MQColor(255, 215, 0), MQColor(255, 215, 0), 6.0f);
		xp.textMode = 0;
		xp.gradientOn = false;
		xp.ticksOn = true;
		xp.tickEvery = 0.10f;
		BarStyle& aa = bar("Inventory", "AAExp", MQColor(120, 170, 230), MQColor(120, 170, 230), 6.0f);
		aa.textMode = 0;
		aa.gradientOn = false;
		aa.ticksOn = true;
		aa.tickEvery = 0.10f;
	}
	for (int p = 0; p < myui::BagSlotCount(); ++p)
	{
		SetFlag("Inventory", "BagLock" + std::to_string(p), false);
	}
	SetStr("Inventory", "ToggleKey", "I");
	SetStr("Inventory", "ToggleMod1", "Ctrl");
	SetStr("Inventory", "ToggleMod2", "None");
	SetStr("Inventory", "ToggleMod3", "None");
	SetStr("Inventory", "ToggleMouse", "None");

	Window("BigBag");
	SetFlag("BigBag", "ShowSlotBackground", true);
	SetFlag("BigBag", "DimUnusable", true);
	SetFlag("BigBag", "SortName", true);
	SetFlag("BigBag", "SortStack", true);
	SetFlag("BigBag", "SortType", true);
	SetNum("BigBag", "ItemSize", 40.0f);
	SetNum("BigBag", "MinSlotsWarn", 3.0f);
	SetStr("BigBag", "ToggleKey", "B");
	SetStr("BigBag", "ToggleMod1", "Ctrl");
	SetStr("BigBag", "ToggleMod2", "None");
	SetStr("BigBag", "ToggleMod3", "None");
	SetStr("BigBag", "ToggleMouse", "None");

	Window("iTrack");
	SetStr("iTrack", "ToggleKey", "T");
	SetStr("iTrack", "ToggleMod1", "Ctrl");
	SetStr("iTrack", "ToggleMod2", "None");
	SetStr("iTrack", "ToggleMod3", "None");
	SetStr("iTrack", "ToggleMouse", "None");

	Window("Themes");
	Window("Settings");
	Window("Copy");

	Window("Toggles").internal = true;
	SetFlag("Toggles", "RightLabel", true);
	SetFlag("Toggles", "GlowOnHover", true);
	SetFlag("Toggles", "PulseOnHover", true);
	SetFlag("Toggles", "StarKnob", false);
	SetFlag("Toggles", "KnobBorder", false);
	SetFlag("Toggles", "AnimateKnob", false);
	SetFlag("Toggles", "AnimateOnHover", false);
	SetNum("Toggles", "Width", 0.0f);
	SetNum("Toggles", "Height", 0.0f);
}

void UiConfig::Init(SettingsStore* store)
{
	m_store = store;
	std::string key = store->Server() + "|" + store->Character();
	if (key != m_loadedKey)
	{
		Load();
		m_loadedKey = key;
	}
}

void UiConfig::Load()
{
	SeedDefaults();
	if (!m_store)
	{
		return;
	}

	for (const std::string& key : m_order)
	{
		WindowConfig& w = m_windows[key];
		w.visible   = m_store->GetBool(key, "Visible", w.visible);
		w.locked    = m_store->GetBool(key, "Locked", w.locked);
		w.textScale = m_store->GetNumber(key, "TextScale", w.textScale);
		w.iconScale = m_store->GetNumber(key, "IconScale", w.iconScale);

		for (auto& [name, val] : w.flags)
		{
			val = m_store->GetBool(key, name, val);
		}
		for (auto& [name, val] : w.nums)
		{
			val = m_store->GetNumber(key, name, val);
		}
		for (auto& [name, val] : w.strs)
		{
			val = m_store->GetString(key, name, val);
		}
		for (auto& [role, style] : w.bars)
		{
			LoadVisitor lv{ m_store, key, "Bar." + role + "." };
			VisitBar(style, lv);
			bool groupPet = (key == "Group" || key == "Raid") && role == "Pet";
			if (!groupPet)
			{
				style.vertical = false;
			}
		}
		for (auto& [role, style] : w.rings)
		{
			LoadVisitor lv{ m_store, key, "Ring." + role + "." };
			VisitRing(style, lv);
		}

		if (IsTransientVisibility(key))
		{
			w.visible = false;
		}
	}

	PruneDefunct();
}

void UiConfig::PruneDefunct()
{
	if (!m_store)
	{
		return;
	}

	static std::set<std::string> barFields = [] {
		std::set<std::string> fields;
		BarStyle dummy;
		KeyCollector collector{ &fields };
		VisitBar(dummy, collector);
		return fields;
	}();
	static std::set<std::string> ringFields = [] {
		std::set<std::string> fields;
		RingStyle dummy;
		KeyCollector collector{ &fields };
		VisitRing(dummy, collector);
		return fields;
	}();
	static const std::set<std::string> coreKeys = { "Visible", "Locked", "TextScale", "IconScale" };
	static const std::set<std::string> externalKeys = { "ActiveTheme" };

	std::vector<SettingRow> rows = m_store->GetAllSettings(m_store->Server(), m_store->Character());
	for (const SettingRow& row : rows)
	{
		auto it = m_windows.find(row.module);
		if (it == m_windows.end())
		{
			continue;
		}
		if (coreKeys.count(row.name) != 0 || externalKeys.count(row.name) != 0)
		{
			continue;
		}
		const WindowConfig& w = it->second;
		if (w.flags.count(row.name) != 0 || w.nums.count(row.name) != 0 || w.strs.count(row.name) != 0)
		{
			continue;
		}
		if (IsValidBarKey(w, barFields, row.name))
		{
			continue;
		}
		if (IsValidRingKey(w, ringFields, row.name))
		{
			continue;
		}
		m_store->DeleteSetting(row.module, row.name);
	}
}

void UiConfig::Save()
{
	if (!m_store)
	{
		return;
	}

	m_store->BeginTransaction();
	for (const std::string& key : m_order)
	{
		WindowConfig& w = m_windows[key];
		m_store->SetBool(key, "Visible", w.visible);
		m_store->SetBool(key, "Locked", w.locked);
		m_store->SetNumber(key, "TextScale", w.textScale);
		m_store->SetNumber(key, "IconScale", w.iconScale);

		for (auto& [name, val] : w.flags)
		{
			m_store->SetBool(key, name, val);
		}
		for (auto& [name, val] : w.nums)
		{
			m_store->SetNumber(key, name, val);
		}
		for (auto& [name, val] : w.strs)
		{
			m_store->SetString(key, name, val);
		}
		for (auto& [role, style] : w.bars)
		{
			SaveVisitor sv{ m_store, key, "Bar." + role + "." };
			VisitBar(style, sv);
		}
		for (auto& [role, style] : w.rings)
		{
			SaveVisitor sv{ m_store, key, "Ring." + role + "." };
			VisitRing(style, sv);
		}
	}
	m_store->CommitTransaction();
	m_store->Flush();
}

void UiConfig::LoadForCharacter(SettingsStore* store, const std::string& server, const std::string& character)
{
	if (!store)
	{
		return;
	}
	std::string curServer = store->Server();
	std::string curChar = store->Character();
	store->SetContext(server, character);
	m_store = store;
	Load();
	m_loadedKey = server + "|" + character;
	store->SetContext(curServer, curChar);
}

void UiConfig::SaveForCharacter(SettingsStore* store, const std::string& server, const std::string& character)
{
	if (!store)
	{
		return;
	}
	std::string curServer = store->Server();
	std::string curChar = store->Character();
	store->SetContext(server, character);
	m_store = store;
	Save();
	store->SetContext(curServer, curChar);
}

void UiConfig::RequestSave()
{
	m_savePending = true;
}

void UiConfig::RequestReload()
{
	m_reloadPending = true;
}

void UiConfig::ProcessPending()
{
	if (m_reloadPending)
	{
		m_reloadPending = false;
		Load();
	}
	if (m_savePending)
	{
		m_savePending = false;
		Save();
	}
}

void UiConfig::PersistVisibility(const std::string& window)
{
	if (!m_store)
	{
		return;
	}
	if (IsTransientVisibility(window))
	{
		return;
	}
	m_store->SetBool(window, "Visible", Window(window).visible);
}

void UiConfig::PersistField(const std::string& window, const std::string& name)
{
	if (!m_store)
	{
		return;
	}
	WindowConfig& w = Window(window);
	if (name == "Visible")
	{
		m_store->SetBool(window, "Visible", w.visible);
	}
	else if (name == "Locked")
	{
		m_store->SetBool(window, "Locked", w.locked);
	}
	else if (name == "TextScale")
	{
		m_store->SetNumber(window, "TextScale", w.textScale);
	}
	else if (name == "IconScale")
	{
		m_store->SetNumber(window, "IconScale", w.iconScale);
	}
	else if (w.flags.count(name))
	{
		m_store->SetBool(window, name, w.flags[name]);
	}
	else if (w.nums.count(name))
	{
		m_store->SetNumber(window, name, w.nums[name]);
	}
}
