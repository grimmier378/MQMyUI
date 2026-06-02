#pragma once

#include "ModuleBase.h"

#include <memory>
#include <string>
#include <vector>

class ModuleManager
{
public:
	void SetContext(const ModuleContext& ctx) { m_ctx = ctx; }

	template <class T>
	T* RegisterModule()
	{
		auto module = std::make_unique<T>();
		T* raw = module.get();
		raw->SetContext(m_ctx);
		m_modules.push_back(std::move(module));
		return raw;
	}

	void InitAll();
	void PulseAll();
	void RenderAll();
	void ShutdownAll();
	void ZoneAll();

	ModuleBase* Find(const std::string& name);
	bool SetEnabled(const std::string& name, bool enabled);

	const std::vector<std::unique_ptr<ModuleBase>>& Modules() const { return m_modules; }

private:
	ModuleContext m_ctx;
	std::vector<std::unique_ptr<ModuleBase>> m_modules;
	bool m_initialized = false;
};
