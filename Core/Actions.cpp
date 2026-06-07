#include "Actions.h"
#include "InventoryData.h"
#include "ChatBridge.h"

#include <mq/Plugin.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace myui
{
namespace
{
int s_pending = 0;
std::chrono::steady_clock::time_point s_deadline;
constexpr float kTradeRange = 20.0f;

enum class SwapStep { None, PreUnequip, WaitTwoHandCursor, WaitPickup, WaitDrop, WaitAuto };
SwapStep    s_swapStep = SwapStep::None;
std::string s_swapPickup;
std::string s_swapDrop;
int         s_swapItemId = 0;
bool        s_swapAuto = false;
std::chrono::steady_clock::time_point s_swapDeadline;
std::chrono::steady_clock::time_point s_swapDropTime;

enum class CoinStep { None, WaitInvOpen, WaitQtyOpen, WaitQtyClosed };
CoinStep s_coinStep = CoinStep::None;
int      s_coinType = 0;
int      s_coinAmount = 0;
std::chrono::steady_clock::time_point s_coinDeadline;

int         s_kickSpawnId = 0;
std::string s_kickName;
std::chrono::steady_clock::time_point s_kickDeadline;

bool WindowVisible(const char* name)
{
	CXWnd* w = FindMQ2Window(name);
	return w && w->IsVisible();
}

void ClickControl(const char* path)
{
	if (CXWnd* w = FindMQ2Window(path))
	{
		SendWndClick2(w, "leftmouseup");
	}
}

enum class TradeStep { None, Nav, Next, WaitPickup, WaitGive, WaitClear, WaitBatch, Final };
TradeStep s_tradeStep = TradeStep::None;
std::vector<std::string> s_tradeQueue;
size_t s_tradeIndex = 0;
int    s_tradePlaced = 0;
int    s_tradeTargetId = 0;
std::chrono::steady_clock::time_point s_tradeDeadline;

void ClickTradeButtons()
{
	if (WindowVisible("TradeWnd"))
	{
		ClickControl("TradeWnd/TRDW_Trade_Button");
	}
	if (WindowVisible("GiveWnd"))
	{
		ClickControl("GiveWnd/GVW_Give_Button");
	}
}

float DistTo(PlayerClient* sp)
{
	if (!sp || !pLocalPlayer)
	{
		return 99999.0f;
	}
	return GetDistance(pLocalPlayer, sp);
}

void Trade(int spawnId)
{
	EzCommand(fmt::format("/target id {}", spawnId).c_str());
	EzCommand("/click left target");
}
} // namespace

void GiveItemTo(int spawnId)
{
	s_pending = 0;
	if (spawnId <= 0)
	{
		return;
	}

	PlayerClient* sp = GetSpawnByID(spawnId);
	if (!sp)
	{
		return;
	}

	EzCommand(fmt::format("/target id {}", spawnId).c_str());

	if (!CursorHasItem())
	{
		return;
	}

	if (DistTo(sp) <= kTradeRange)
	{
		Trade(spawnId);
		return;
	}

	EzCommand(fmt::format("/nav id {}", spawnId).c_str());
	s_pending = spawnId;
	s_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
}

void SwapToWornSlot(const std::string& pickupCommand, const std::string& dropCommand, bool autoInventory,
	const std::string& preUnequipCommand, int swapItemId)
{
	if (dropCommand.empty())
	{
		return;
	}

	s_swapPickup = pickupCommand;
	s_swapDrop = dropCommand;
	s_swapItemId = swapItemId;
	s_swapAuto = autoInventory;
	s_swapDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

	if (!preUnequipCommand.empty() && !pickupCommand.empty() && !CursorHasItem())
	{
		s_swapAuto = true;
		EzCommand(preUnequipCommand.c_str());
		s_swapStep = SwapStep::PreUnequip;
	}
	else if (!pickupCommand.empty() && !CursorHasItem())
	{
		EzCommand(pickupCommand.c_str());
		s_swapStep = SwapStep::WaitPickup;
	}
	else if (CursorHasItem())
	{
		EzCommand(dropCommand.c_str());
		s_swapDropTime = std::chrono::steady_clock::now();
		s_swapStep = SwapStep::WaitDrop;
	}
}

void PulseSwap()
{
	if (s_swapStep == SwapStep::None)
	{
		return;
	}

	auto now = std::chrono::steady_clock::now();
	if (now > s_swapDeadline)
	{
		s_swapStep = SwapStep::None;
		return;
	}

	switch (s_swapStep)
	{
	case SwapStep::PreUnequip:
		if (CursorHasItem())
		{
			EzCommand(s_swapPickup.c_str());
			s_swapStep = SwapStep::WaitTwoHandCursor;
		}
		break;

	case SwapStep::WaitTwoHandCursor:
	{
		ItemRef cursor = CursorItem();
		if (cursor.valid() && cursor.id() == s_swapItemId)
		{
			EzCommand(s_swapDrop.c_str());
			s_swapDropTime = now;
			s_swapStep = SwapStep::WaitDrop;
		}
		break;
	}

	case SwapStep::WaitPickup:
		if (CursorHasItem())
		{
			EzCommand(s_swapDrop.c_str());
			s_swapDropTime = now;
			s_swapStep = SwapStep::WaitDrop;
		}
		break;

	case SwapStep::WaitDrop:
		if (now - s_swapDropTime > std::chrono::milliseconds(150))
		{
			if (s_swapAuto && CursorHasItem())
			{
				EzCommand("/autoinventory");
				s_swapStep = SwapStep::WaitAuto;
			}
			else
			{
				s_swapStep = SwapStep::None;
			}
		}
		break;

	case SwapStep::WaitAuto:
		if (!CursorHasItem())
		{
			s_swapStep = SwapStep::None;
		}
		break;

	default:
		s_swapStep = SwapStep::None;
		break;
	}
}

void PickupCoin(int coinType, int amount)
{
	if (coinType < 0 || coinType > 3 || amount <= 0)
	{
		return;
	}
	s_coinType = coinType;
	s_coinAmount = amount;
	s_coinDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
	if (CXWnd* inv = FindMQ2Window("InventoryWindow"))
	{
		inv->Show(true, true);
	}
	s_coinStep = CoinStep::WaitInvOpen;
}

void PulseCoin()
{
	if (s_coinStep == CoinStep::None)
	{
		return;
	}
	if (std::chrono::steady_clock::now() > s_coinDeadline)
	{
		s_coinStep = CoinStep::None;
		return;
	}

	switch (s_coinStep)
	{
	case CoinStep::WaitInvOpen:
		if (WindowVisible("InventoryWindow"))
		{
			ClickControl(fmt::format("InventoryWindow/IW_Money{}", s_coinType).c_str());
			s_coinStep = CoinStep::WaitQtyOpen;
		}
		break;

	case CoinStep::WaitQtyOpen:
		if (WindowVisible("QuantityWnd"))
		{
			if (CXWnd* input = FindMQ2Window("QuantityWnd/QTYW_SliderInput"))
			{
				char buf[32];
				sprintf_s(buf, "%d", s_coinAmount);
				input->SetWindowText(CXStr(buf));
				input->ParentWndNotification(input, XWM_NEWVALUE, nullptr);
			}
			ClickControl("QuantityWnd/QTYW_Accept_Button");
			s_coinStep = CoinStep::WaitQtyClosed;
		}
		break;

	case CoinStep::WaitQtyClosed:
		if (!WindowVisible("QuantityWnd"))
		{
			if (CXWnd* inv = FindMQ2Window("InventoryWindow"))
			{
				inv->Show(false, true);
			}
			s_coinStep = CoinStep::None;
		}
		break;

	default:
		s_coinStep = CoinStep::None;
		break;
	}
}

void StartBulkTrade(const std::vector<std::string>& itemNames)
{
	if (itemNames.empty())
	{
		return;
	}
	if (!pTarget || pTarget == pLocalPlayer)
	{
		ChatOutf("\ar[MyUI]\ax Select another character as your trade target first.");
		return;
	}

	s_tradeQueue = itemNames;
	s_tradeIndex = 0;
	s_tradePlaced = 0;
	s_tradeTargetId = pTarget->SpawnID;
	s_tradeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

	EzCommand(fmt::format("/target id {}", s_tradeTargetId).c_str());

	if (DistTo(pTarget) > kTradeRange)
	{
		EzCommand(fmt::format("/nav id {} dist=12", s_tradeTargetId).c_str());
		s_tradeStep = TradeStep::Nav;
	}
	else
	{
		s_tradeStep = TradeStep::Next;
	}
}

void PulseTrade()
{
	if (s_tradeStep == TradeStep::None)
	{
		return;
	}

	auto now = std::chrono::steady_clock::now();
	PlayerClient* target = GetSpawnByID(s_tradeTargetId);
	if (!target)
	{
		s_tradeStep = TradeStep::None;
		return;
	}

	switch (s_tradeStep)
	{
	case TradeStep::Nav:
		if (DistTo(target) <= kTradeRange || now > s_tradeDeadline)
		{
			EzCommand("/nav stop");
			s_tradeStep = TradeStep::Next;
		}
		break;

	case TradeStep::Next:
		if (s_tradeIndex >= s_tradeQueue.size())
		{
			s_tradeDeadline = now + std::chrono::seconds(5);
			s_tradeStep = TradeStep::Final;
			break;
		}
		{
			myui::ItemRef item = myui::FindBagItemByName(s_tradeQueue[s_tradeIndex]);
			++s_tradeIndex;
			if (item.valid())
			{
				EzCommand(("/shift " + myui::PickupCommand(item)).c_str());
				s_tradeDeadline = now + std::chrono::seconds(5);
				s_tradeStep = TradeStep::WaitPickup;
			}
		}
		break;

	case TradeStep::WaitPickup:
		if (now > s_tradeDeadline)
		{
			s_tradeStep = TradeStep::Next;
			break;
		}
		if (CursorHasItem())
		{
			if (myui::CursorItem().isContainer())
			{
				EzCommand("/autoinventory");
				s_tradeDeadline = now + std::chrono::seconds(5);
				s_tradeStep = TradeStep::WaitClear;
			}
			else
			{
				EzCommand("/click left target");
				s_tradeDeadline = now + std::chrono::seconds(5);
				s_tradeStep = TradeStep::WaitGive;
			}
		}
		break;

	case TradeStep::WaitClear:
		if (now > s_tradeDeadline || !CursorHasItem())
		{
			s_tradeStep = TradeStep::Next;
		}
		break;

	case TradeStep::WaitGive:
		if (now > s_tradeDeadline)
		{
			s_tradeStep = TradeStep::Next;
			break;
		}
		if (!CursorHasItem())
		{
			++s_tradePlaced;
			if (s_tradePlaced % 8 == 0)
			{
				ClickTradeButtons();
				s_tradeDeadline = now + std::chrono::seconds(5);
				s_tradeStep = TradeStep::WaitBatch;
			}
			else
			{
				s_tradeStep = TradeStep::Next;
			}
		}
		break;

	case TradeStep::WaitBatch:
		if (now > s_tradeDeadline || !WindowVisible("TradeWnd"))
		{
			s_tradeStep = TradeStep::Next;
		}
		break;

	case TradeStep::Final:
		ClickTradeButtons();
		s_tradeStep = TradeStep::None;
		break;

	default:
		s_tradeStep = TradeStep::None;
		break;
	}
}

void PulseGive()
{
	if (s_pending <= 0)
	{
		return;
	}

	if (std::chrono::steady_clock::now() > s_deadline || !CursorHasItem())
	{
		s_pending = 0;
		return;
	}

	PlayerClient* sp = GetSpawnByID(s_pending);
	if (!sp)
	{
		s_pending = 0;
		return;
	}

	if (DistTo(sp) <= kTradeRange)
	{
		EzCommand("/nav stop");
		Trade(s_pending);
		s_pending = 0;
	}
}

void KickFromGroup(int spawnId, const std::string& name)
{
	s_kickSpawnId = spawnId;
	s_kickName = name;
	s_kickDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	if (spawnId > 0)
	{
		EzCommand(fmt::format("/target id {}", spawnId).c_str());
	}
	else if (!name.empty())
	{
		EzCommand(fmt::format("/target {}", name).c_str());
	}
}

void PulseKick()
{
	if (s_kickSpawnId == 0 && s_kickName.empty())
	{
		return;
	}
	if (std::chrono::steady_clock::now() > s_kickDeadline)
	{
		s_kickSpawnId = 0;
		s_kickName.clear();
		return;
	}
	if (!pTarget)
	{
		return;
	}
	bool match = s_kickSpawnId > 0
		? (static_cast<int>(pTarget->SpawnID) == s_kickSpawnId)
		: ci_equals(pTarget->DisplayedName, s_kickName);
	if (match)
	{
		EzCommand("/disband");
		s_kickSpawnId = 0;
		s_kickName.clear();
	}
}

void PulseActions()
{
	PulseGive();
	PulseSwap();
	PulseCoin();
	PulseTrade();
	PulseKick();
}
} // namespace myui
