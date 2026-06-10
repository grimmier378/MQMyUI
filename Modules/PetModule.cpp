#include "PetModule.h"

#include "../Core/UiConfig.h"
#include "../Core/BarEngine.h"
#include "../Core/ThemeManager.h"
#include "../Core/CharData.h"
#include "../Core/IconHelper.h"
#include "../Core/UiHelpers.h"
#include "../Core/Actions.h"
#include "../Core/Widgets.h"

#include <cstring>

namespace
{
struct PetCommand
{
	const char* label;
	const char* command;
	const char* stateTlo;
	const char* activeWhen;
};

const PetCommand kPetCommands[] = {
	{ "Attack",  "/pet attack",     nullptr,           nullptr },
	{ "Back",    "/pet back off",   nullptr,           nullptr },
	{ "Guard",   "/pet guard here", nullptr,           nullptr },
	{ "Follow",  "/pet follow",     "${Pet.Stance}",   "FOLLOW" },
	{ "Sit",     "/pet sit",        "${Pet.Sitting}",  "TRUE" },
	{ "Stop",    "/pet stop",       nullptr,           nullptr },
	{ "Taunt",   "/pet taunt",      "${Pet.Taunt}",    "TRUE" },
	{ "Hold",    "/pet hold on",    nullptr,           nullptr },
	{ "NoHold",  "/pet hold off",   nullptr,           nullptr },
	{ "Kill",    "/pet kill",       nullptr,           nullptr },
	{ "Health",  "/pet health",     nullptr,           nullptr },
};

bool PetToggleActive(const PetCommand& cmd)
{
	if (!cmd.stateTlo)
	{
		return false;
	}
	char buf[MAX_STRING] = { 0 };
	strcpy_s(buf, cmd.stateTlo);
	ParseMacroData(buf, sizeof(buf));
	return ci_equals(buf, cmd.activeWhen);
}

const MQColor kTint(255, 255, 255, 255);
} // namespace

void PetModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !pLocalPlayer)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Pet##MyUIPet", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		PlayerClient* pPet = GetSpawnByID(pLocalPlayer->PetID);
		if (!pPet)
		{
			ImGui::TextDisabled("No Pet");
		}
		else
		{
			std::string petName = mq::IsAnonymized() ? "Pet" : myui::TrimName(pPet->DisplayedName);
			ImGui::TextUnformatted(petName.c_str());
			myui::DrawStyledBar("##pethp", static_cast<float>(pPet->HPCurrent), m_ctx.UI->Bar(GetName(), "HP"));
			if (ImGui::IsItemClicked())
			{
				myui::GiveItemTo(pLocalPlayer->PetID);
			}

			if (PlayerClient* pPetTarget = pPet->WhoFollowing)
			{
				std::string targetName = myui::DisplayedSpawnName(pPetTarget);
				ImGui::Text("Target: %s", targetName.c_str());
				myui::DrawStyledBar("##pettht", static_cast<float>(pPetTarget->HPCurrent), m_ctx.UI->Bar(GetName(), "PetTarget"));
			}
			else
			{
				ImGui::TextUnformatted("Target: No Target");
				myui::DrawStyledBar("##pettht", 0.0f, m_ctx.UI->Bar(GetName(), "PetTarget"));
			}

			ImGui::Separator();

			ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable
				| ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp;

			if (ImGui::BeginTable("##PetLayout", 2, tableFlags))
			{
				ImGui::TableSetupColumn("Buttons", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Buffs", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				bool col0Enabled = (ImGui::TableGetColumnFlags(0) & ImGuiTableColumnFlags_IsEnabled) != 0;
				bool col1Enabled = (ImGui::TableGetColumnFlags(1) & ImGuiTableColumnFlags_IsEnabled) != 0;
				if (!col0Enabled && !col1Enabled)
				{
					ImGui::TableSetColumnEnabled(0, true);
				}

				ImGui::TableNextRow();
				if (ImGui::TableNextColumn())
				{
					DrawPetButtons();
				}
				if (ImGui::TableNextColumn())
				{
					DrawPetBuffs();
				}

				ImGui::EndTable();
			}
		}

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}
}

void PetModule::DrawPetButtons()
{
	float scale = m_ctx.UI->Window(GetName()).textScale;
	float avail = ImGui::GetContentRegionAvail().x;
	float buttonWidth = 56.0f * scale;
	int perRow = static_cast<int>(avail / (buttonWidth + 4.0f));
	if (perRow < 1)
	{
		perRow = 1;
	}

	int col = 0;
	for (const PetCommand& cmd : kPetCommands)
	{
		if (!m_ctx.UI->Flag(GetName(), cmd.label, true))
		{
			continue;
		}

		if (col > 0 && (col % perRow) != 0)
		{
			ImGui::SameLine();
		}

		bool active = PetToggleActive(cmd);
		if (active)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
		}

		if (myui::StyledButton(cmd.label, ImVec2(buttonWidth, 0)))
		{
			EzCommand(cmd.command);
		}

		if (active)
		{
			ImGui::PopStyleColor();
		}

		++col;
	}
}

void PetModule::DrawPetBuffs()
{
	if (!m_ctx.Icons || !pPetInfoWnd)
	{
		return;
	}

	int iconSize = static_cast<int>(22.0f * m_ctx.UI->Window(GetName()).iconScale);
	int lineWidth = 0;
	int avail = static_cast<int>(ImGui::GetContentRegionAvail().x);
	bool first = true;

	for (const auto& buffInfo : pPetInfoWnd->GetBuffRange())
	{
		EQ_Spell* spell = buffInfo.GetSpell();
		if (!spell)
		{
			continue;
		}

		ImGui::PushID(spell->ID);
		m_ctx.Icons->DrawSpellIcon(spell->SpellIcon, CXSize(iconSize, iconSize), kTint, myui::kBuffBeneficial);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetItemTooltip("%s", spell->Name);
		}
		ImGui::PopID();

		lineWidth += iconSize + 2;
		if (lineWidth < avail - 20)
		{
			ImGui::SameLine(0.0f, 2.0f);
		}
		else
		{
			lineWidth = 0;
		}

		first = false;
	}

	if (first)
	{
		ImGui::TextDisabled("No Pet Buffs");
	}
}
