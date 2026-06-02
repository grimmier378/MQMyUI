#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/AAData.h"

#include <mq/Plugin.h>

class MyAAModule : public ModuleBase
{
public:
	const char* GetName() const override { return "MyAA"; }

	void OnRenderGUI() override;
	void OnPulse() override;

private:
	void DrawTable(int type, float height);

	AAData m_aaData;
	int m_selected = -1;
	int m_activeType = 1;

	int m_pendingBuy = -1;
	int m_pendingHotkey = -1;
	int m_confirmGroup = -1;
	bool m_openConfirm = false;
	char m_search[64] = { 0 };
};
