#include <mq/Plugin.h>

#include "Core/SettingsStore.h"
#include "Core/ThemeManager.h"
#include "Core/CharData.h"
#include "Core/ChatBridge.h"
#include "Core/IconHelper.h"
#include "Core/ActorManager.h"
#include "Core/ModuleManager.h"
#include "Core/UiConfig.h"
#include "Core/Actions.h"
#include "Core/InventoryData.h"

#include "Modules/PlayerModule.h"
#include "Modules/HudModule.h"
#include "Modules/PetModule.h"
#include "Modules/BuffsModule.h"
#include "Modules/SongsModule.h"
#include "Modules/CastingModule.h"
#include "Modules/SpellsModule.h"
#include "Modules/GroupModule.h"
#include "Modules/XpBarsModule.h"
#include "Modules/MyAAModule.h"
#include "Modules/ThemeZModule.h"
#include "Modules/SettingsModule.h"
#include "Modules/CopyModule.h"
#include "Modules/MyInventoryModule.h"
#include "Modules/BigBagModule.h"
#include "Modules/ITrackModule.h"
#include "Modules/ToolbarModule.h"

#include <chrono>
#include <filesystem>

PreSetup("MQMyUI");
PLUGIN_VERSION(0.1);

static SettingsStore  s_settings;
static ThemeManager   s_themes;
static CharData        s_charData;
static ChatBridge      s_chat;
static IconHelper      s_icons;
static ActorManager    s_actors;
static UiConfig        s_ui;
static ModuleManager  s_modules;

static bool s_charInitialized = false;
static std::chrono::steady_clock::time_point s_pulseTimer;
static std::chrono::steady_clock::time_point s_flushTimer;

static void RegisterModules()
{
	ModuleContext ctx;
	ctx.Settings = &s_settings;
	ctx.Themes   = &s_themes;
	ctx.Char     = &s_charData;
	ctx.Chat     = &s_chat;
	ctx.Icons    = &s_icons;
	ctx.Actors   = &s_actors;
	ctx.UI       = &s_ui;

	s_modules.SetContext(ctx);
	s_modules.RegisterModule<PlayerModule>();
	s_modules.RegisterModule<PetModule>();
	s_modules.RegisterModule<BuffsModule>();
	s_modules.RegisterModule<SongsModule>();
	s_modules.RegisterModule<SpellsModule>();
	s_modules.RegisterModule<CastingModule>();
	s_modules.RegisterModule<HudModule>();
	s_modules.RegisterModule<GroupModule>();
	s_modules.RegisterModule<XpBarsModule>();
	s_modules.RegisterModule<MyAAModule>();
	s_modules.RegisterModule<ThemeZModule>();
	s_modules.RegisterModule<SettingsModule>();
	s_modules.RegisterModule<CopyModule>();
	s_modules.RegisterModule<MyInventoryModule>();
	s_modules.RegisterModule<BigBagModule>();
	s_modules.RegisterModule<ITrackModule>();
	s_modules.RegisterModule<ToolbarModule>();
}

static void InitForCharacter()
{
	if (!pLocalPC)
	{
		return;
	}

	if (!s_settings.IsOpen())
	{
		std::string dir = fmt::format("{}/MQMyUI", gPathConfig);
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);

		std::string dbPath = fmt::format("{}/MQMyUI.db", dir);
		if (!s_settings.Open(dbPath))
		{
			return;
		}
	}

	const char* serverName = GetServerShortName();
	s_settings.SetContext(serverName ? serverName : "", pLocalPC->Name);
	s_themes.Init(&s_settings);
	s_themes.LoadActiveTheme();
	s_ui.Init(&s_settings);

	s_actors.Init(&s_charData, &s_chat, &s_settings, serverName ? serverName : "", pLocalPC->Name);

	s_charData.Refresh();
	s_modules.InitAll();

	s_charInitialized = true;
	s_flushTimer = std::chrono::steady_clock::now() + std::chrono::seconds(5);
}

static void TeardownCharacter()
{
	if (!s_charInitialized)
	{
		return;
	}

	s_modules.ShutdownAll();
	s_actors.Shutdown();
	s_settings.Flush();
	s_charInitialized = false;
}

static bool MatchWindow(const char* in, std::string& out)
{
	for (const std::string& key : s_ui.WindowKeys())
	{
		if (ci_equals(key, in))
		{
			out = key;
			return true;
		}
	}
	return false;
}

static void MyUICommand(PlayerClient*, const char* Line)
{
	char arg1[MAX_STRING] = { 0 };
	GetArg(arg1, Line, 1);

	if (arg1[0] == 0 || ci_equals(arg1, "help"))
	{
		myui::ChatOutf("\ag[MyUI]\ax commands:");
		myui::ChatOutf("  \ay/myui list\ax - list windows and visibility");
		myui::ChatOutf("  \ay/myui <window>\ax - toggle a window");
		myui::ChatOutf("  \ay/myui show|hide <window>\ax - show/hide a window");
		myui::ChatOutf("  \ay/myui settings\ax - open the settings window");
		myui::ChatOutf("  \ay/myui theme\ax - open the theme editor");
		myui::ChatOutf("  \ay/myui theme set <name>\ax - set the active theme");
		myui::ChatOutf("  \ay/myui theme list\ax - list available themes");
		return;
	}

	if (ci_equals(arg1, "list"))
	{
		myui::ChatOutf("\ag[MyUI]\ax windows:");
		for (const std::string& key : s_ui.WindowKeys())
		{
			myui::ChatOutf("  %s - %s", key.c_str(), s_ui.Window(key).visible ? "\agshown\ax" : "\arhidden\ax");
		}
		return;
	}

	if (ci_equals(arg1, "show") || ci_equals(arg1, "hide"))
	{
		char arg2[MAX_STRING] = { 0 };
		GetArg(arg2, Line, 2);
		std::string key;
		if (MatchWindow(arg2, key))
		{
			s_ui.Window(key).visible = ci_equals(arg1, "show");
			s_ui.PersistVisibility(key);
		}
		else
		{
			myui::ChatOutf("\ar[MyUI]\ax unknown window '%s'", arg2);
		}
		return;
	}

	if (ci_equals(arg1, "theme"))
	{
		char arg2[MAX_STRING] = { 0 };
		GetArg(arg2, Line, 2);

		if (arg2[0] == 0)
		{
			s_ui.ToggleVisible("ThemeZ");
			return;
		}

		if (ci_equals(arg2, "list"))
		{
			myui::ChatOutf("\ag[MyUI]\ax themes:");
			for (const std::string& name : s_themes.GetThemeNames())
			{
				bool active = ci_equals(name, s_themes.ActiveName());
				myui::ChatOutf("  %s%s", name.c_str(), active ? " \ag(active)\ax" : "");
			}
			return;
		}

		if (ci_equals(arg2, "set"))
		{
			char arg3[MAX_STRING] = { 0 };
			GetArg(arg3, Line, 3);
			if (arg3[0] == 0)
			{
				myui::ChatOutf("\ar[MyUI]\ax usage: /myui theme set <name>");
				return;
			}

			for (const std::string& name : s_themes.GetThemeNames())
			{
				if (ci_equals(name, arg3))
				{
					s_themes.SetActiveTheme(name);
					myui::ChatOutf("\ag[MyUI]\ax theme set to %s", name.c_str());
					return;
				}
			}
			myui::ChatOutf("\ar[MyUI]\ax unknown theme '%s'", arg3);
			return;
		}

		myui::ChatOutf("\ar[MyUI]\ax usage: /myui theme | theme set <name> | theme list");
		return;
	}

	std::string key;
	if (MatchWindow(arg1, key))
	{
		s_ui.ToggleVisible(key);
	}
	else
	{
		myui::ChatOutf("\ar[MyUI]\ax unknown window '%s'", arg1);
	}
}

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("MQMyUI::Initializing version %f", MQ2Version);

	AddCommand("/myui", MyUICommand, false, true, true);
	RegisterModules();
	s_actors.SetReloadHandler([]() { s_settings.InvalidateCache(); s_ui.Load(); });
	s_chat.DetectMyChat();
}

PLUGIN_API void ShutdownPlugin()
{
	TeardownCharacter();
	s_icons.Shutdown();
	s_settings.Close();
	RemoveCommand("/myui");
}

PLUGIN_API void SetGameState(int GameState)
{
	if (GameState != GAMESTATE_INGAME)
	{
		TeardownCharacter();
	}
}

PLUGIN_API void OnPulse()
{
	if (GetGameState() != GAMESTATE_INGAME || !pLocalPC)
	{
		return;
	}

	if (!s_charInitialized)
	{
		InitForCharacter();
	}

	myui::PulseActions();
	s_ui.ProcessPending();

	auto now = std::chrono::steady_clock::now();
	if (now < s_pulseTimer)
	{
		return;
	}
	s_pulseTimer = now + std::chrono::milliseconds(250);

	s_charData.Refresh();
	s_modules.PulseAll();
	s_actors.Pulse();
	myui::PulseAugInsert();

	if (now >= s_flushTimer)
	{
		s_settings.Flush();
		s_flushTimer = now + std::chrono::seconds(5);
	}
}

PLUGIN_API void OnUpdateImGui()
{
	if (GetGameState() != GAMESTATE_INGAME || !s_charInitialized)
	{
		return;
	}

	ImGuiStyle saved = s_themes.ApplyTheme(s_themes.Resolved());
	s_modules.RenderAll();
	myui::DrawPoppedInfoWindows(&s_icons);
	ThemeManager::ResetTheme(saved);
}

PLUGIN_API void OnZoned()
{
	if (s_charInitialized)
	{
		s_modules.ZoneAll();
	}
}

PLUGIN_API void OnLoadPlugin(const char* Name)
{
	if (ci_equals(Name, "MQMyChat"))
	{
		s_chat.SetMyChatPresent(true);
	}
}

PLUGIN_API void OnUnloadPlugin(const char* Name)
{
	if (ci_equals(Name, "MQMyChat"))
	{
		s_chat.SetMyChatPresent(false);
	}
}
