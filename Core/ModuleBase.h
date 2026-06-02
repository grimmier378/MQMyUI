#pragma once

#include <cassert>
#include <string>

class SettingsStore;
class ThemeManager;
class CharData;
class ChatBridge;
class ActorManager;
class IconHelper;
class UiConfig;

struct ModuleContext
{
	SettingsStore* Settings = nullptr;
	ThemeManager*  Themes   = nullptr;
	CharData*      Char     = nullptr;
	ChatBridge*    Chat     = nullptr;
	ActorManager*  Actors   = nullptr;
	IconHelper*    Icons    = nullptr;
	UiConfig*      UI       = nullptr;
};

class ModuleBase
{
public:
	virtual ~ModuleBase() = default;

	virtual const char* GetName() const = 0;

	virtual void OnInit() {}
	virtual void OnPulse() {}
	virtual void OnRenderGUI() {}
	virtual void OnShutdown() {}
	virtual void OnZone() {}

	bool IsEnabled = true;

protected:
	ModuleContext m_ctx;

	friend class ModuleManager;
	void SetContext(const ModuleContext& ctx) { m_ctx = ctx; }
};
