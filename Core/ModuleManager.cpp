#include "ModuleManager.h"

#include <mq/Plugin.h>

void ModuleManager::InitAll()
{
	for (auto& module : m_modules)
	{
		if (module->IsEnabled)
		{
			module->OnInit();
		}
	}
	m_initialized = true;
}

void ModuleManager::PulseAll()
{
	if (!m_initialized)
	{
		return;
	}

	for (auto& module : m_modules)
	{
		if (module->IsEnabled)
		{
			module->OnPulse();
		}
	}
}

void ModuleManager::RenderAll()
{
	if (!m_initialized)
	{
		return;
	}

	for (auto& module : m_modules)
	{
		if (module->IsEnabled)
		{
			module->OnRenderGUI();
		}
	}
}

void ModuleManager::ShutdownAll()
{
	if (!m_initialized)
	{
		return;
	}

	for (auto& module : m_modules)
	{
		module->OnShutdown();
	}

	m_initialized = false;
}

void ModuleManager::ZoneAll()
{
	if (!m_initialized)
	{
		return;
	}

	for (auto& module : m_modules)
	{
		if (module->IsEnabled)
		{
			module->OnZone();
		}
	}
}

ModuleBase* ModuleManager::Find(const std::string& name)
{
	for (auto& module : m_modules)
	{
		if (ci_equals(module->GetName(), name))
		{
			return module.get();
		}
	}
	return nullptr;
}

bool ModuleManager::SetEnabled(const std::string& name, bool enabled)
{
	ModuleBase* module = Find(name);
	if (!module)
	{
		return false;
	}

	if (module->IsEnabled == enabled)
	{
		return true;
	}

	module->IsEnabled = enabled;

	if (m_initialized)
	{
		if (enabled)
		{
			module->OnInit();
		}
		else
		{
			module->OnShutdown();
		}
	}

	return true;
}
