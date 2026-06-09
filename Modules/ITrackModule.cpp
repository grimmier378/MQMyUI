#include "ITrackModule.h"

#include "../Core/UiConfig.h"
#include "../Core/HotkeyCombo.h"
#include "../Core/ActorManager.h"
#include "../Core/InventoryData.h"
#include "../Core/CharData.h"
#include "../Core/Widgets.h"

void ITrackModule::OnRenderGUI()
{
	UiConfig* ui = m_ctx.UI;
	if (!ui || !m_ctx.Actors)
	{
		return;
	}

	WindowConfig& w = ui->Window(GetName());

	if (myui::CheckToggleCombo(myui::ReadCombo(w)))
	{
		w.visible = !w.visible;
		ui->PersistVisibility(GetName());
	}

	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame())
	{
		return;
	}

	ImGuiWindowFlags flags = 0;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	ImGui::SetNextWindowSize(ImVec2(560.0f, 400.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI iTrack##MyUIiTrack", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		ImGui::SetNextItemWidth(220.0f);
		bool entered = ImGui::InputTextWithHint("##additem", "Enter Item Name...", m_input, sizeof(m_input),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		if (myui::StyledButton("Add") || entered)
		{
			if (m_input[0] != 0)
			{
				m_ctx.Actors->AddTrackedItem(m_input);
				m_input[0] = 0;
			}
		}
		if (myui::CursorHasItem())
		{
			ImGui::SameLine();
			if (myui::StyledButton("Add Cursor Item"))
			{
				myui::ItemRef cursor = myui::CursorItem();
				if (cursor.valid())
				{
					m_ctx.Actors->AddTrackedItem(cursor.name());
					EzCommand("/autoinventory");
				}
			}
		}

		ImGui::Separator();

		ImGui::BeginChild("##ItemList", ImVec2(170.0f, 0.0f), ImGuiChildFlags_Borders);
		DrawItemList();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##ItemDetails");
		DrawItemDetails();
		ImGui::EndChild();

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		ui->PersistVisibility(GetName());
	}
}

void ITrackModule::DrawItemList()
{
	for (const std::string& item : m_ctx.Actors->TrackedItems())
	{
		bool hasAny = false;
		if (const auto* counts = m_ctx.Actors->CountsForItem(item))
		{
			for (const auto& [character, count] : *counts)
			{
				if (count.inventory + count.bank > 0)
				{
					hasAny = true;
					break;
				}
			}
		}

		ImGui::PushStyleColor(ImGuiCol_Text, hasAny ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		if (ImGui::Selectable(item.c_str(), item == m_selected))
		{
			m_selected = item;
		}
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Right-click to remove");
		}
		if (ImGui::BeginPopupContextItem((item + "##ctx").c_str()))
		{
			if (ImGui::MenuItem(("Remove " + item).c_str()))
			{
				m_ctx.Actors->RemoveTrackedItem(item);
				if (m_selected == item)
				{
					m_selected.clear();
				}
			}
			ImGui::EndPopup();
		}
	}
}

void ITrackModule::DrawItemDetails()
{
	if (m_selected.empty())
	{
		ImGui::TextDisabled("Select an item to view details.");
		return;
	}

	ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%s", m_selected.c_str());

	const auto* counts = m_ctx.Actors->CountsForItem(m_selected);
	int total = 0;
	if (counts)
	{
		for (const auto& [character, count] : *counts)
		{
			total += count.inventory + count.bank;
		}
	}
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Count: %d", total);

	ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("##counts", 3, tableFlags))
	{
		ImGui::TableSetupColumn("Character");
		ImGui::TableSetupColumn("Inventory");
		ImGui::TableSetupColumn("Bank");
		ImGui::TableHeadersRow();

		if (counts)
		{
			for (const auto& [character, count] : *counts)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				std::string disp = character;
				if (mq::IsAnonymized())
				{
					if (pLocalPC && ci_equals(character, pLocalPC->Name))
					{
						disp = "Me";
					}
					else if (const myui::PeerRecord* pr = m_ctx.Actors ? m_ctx.Actors->FindPeerByName(character) : nullptr)
					{
						disp = pr->maskedName;
					}
					else
					{
						disp = "Player";
					}
				}
				// Display the masked code but keep the real name as the id + switch target.
				std::string label = disp + "##" + character;
				if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
				{
					m_ctx.Actors->SwitchTo(character);
				}
				ImGui::TableNextColumn();
				ImGui::TextColored(count.inventory > 0 ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
					"%d", count.inventory);
				ImGui::TableNextColumn();
				ImGui::TextColored(count.bank > 0 ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
					"%d", count.bank);
			}
		}

		ImGui::EndTable();
	}
}
