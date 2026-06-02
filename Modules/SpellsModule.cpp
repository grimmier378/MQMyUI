#include "SpellsModule.h"

#include "../Core/UiConfig.h"
#include "../Core/ThemeManager.h"
#include "../Core/CharData.h"
#include "../Core/IconHelper.h"
#include "../Core/SpellInfo.h"

#include "mq/imgui/Widgets.h"
#include "imgui/fonts/IconsFontAwesome.h"

namespace
{
const MQColor kTintReady(255, 255, 255, 255);
const MQColor kTintNotReady(70, 70, 70, 255);

bool GemReady(int i)
{
	if (!pLocalPlayer || !pLocalPC || !pDisplay)
	{
		return false;
	}
	if (!GetSpellByID(pLocalPC->GetMemorizedSpell(i)))
	{
		return false;
	}
	return pDisplay->TimeStamp > pLocalPlayer->SpellGemETA[i]
		&& pDisplay->TimeStamp > pLocalPlayer->GetSpellCooldownETA();
}
} // namespace

int SpellsModule::GemCount() const
{
	int count = 8;
	int aaIndex = GetAAIndexByName("Mnemonic Retention");
	if (CAltAbilityData* pAbility = GetAAById(aaIndex))
	{
		if (PlayerHasAAAbility(aaIndex))
		{
			count += pAbility->CurrentRank;
		}
	}
	return count;
}

void SpellsModule::OnInit()
{
	m_picker.InitializeSpells();
}

void SpellsModule::OnPulse()
{
	if (m_picker.m_selectedSpell && m_memGemIndex > 0)
	{
		DoCommandf("/memspell %d \"%s\"", m_memGemIndex, m_picker.m_selectedSpell->Name);
		m_picker.ClearSelection();
		m_memGemIndex = 0;
	}

	if (m_picker.m_needFilter)
	{
		m_picker.FilterSpells();
	}
}

void SpellsModule::OpenPickerForGem(int gemIndex)
{
	m_memGemIndex = gemIndex + 1;
	m_picker.m_filterString.clear();
	m_picker.m_needFilter = true;
	m_picker.SetOpen(true);
}

void SpellsModule::OnRenderGUI()
{
	WindowConfig& w = m_ctx.UI->Window(GetName());
	if (!w.visible || !m_ctx.Char || !m_ctx.Char->IsIngame() || !m_ctx.Icons || !pLocalPC)
	{
		return;
	}

	bool vertical = m_ctx.UI->Flag(GetName(), "Vertical", false);
	float gemHeight = m_ctx.UI->Num(GetName(), "GemHeight", 32.0f);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
	if (w.locked)
	{
		flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
	}

	if (ImGui::Begin("MyUI Spells##MyUISpells", &w.visible, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * w.textScale);

		const int gemCount = GemCount();
		float scaledGem = gemHeight * w.iconScale;
		int iconPx = static_cast<int>(scaledGem * 0.75f);
		const CXSize gemSize(static_cast<int>(scaledGem * 1.25f), static_cast<int>(scaledGem));

		for (int i = 0; i < gemCount; ++i)
		{
			int spellId = pLocalPC->GetMemorizedSpell(i);
			if (spellId == 0)
			{
				continue;
			}

			ImGui::PushID(i);

			EQ_Spell* spell = (spellId > 0) ? GetSpellByID(spellId) : nullptr;
			CSpellGemWnd* gem = pCastSpellWnd ? pCastSpellWnd->SpellSlots[i] : nullptr;

			if (!spell)
			{
				if (CTextureAnimation* holder = m_ctx.Icons->GemHolder())
				{
					holder->SetCurCell(0);
					imgui::DrawTextureAnimation(holder, gemSize);
				}
				else
				{
					ImGui::Dummy(ImVec2(static_cast<float>(scaledGem), static_cast<float>(scaledGem)));
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetItemTooltip("Empty gem - right-click to memorize");
				}
			}
			else
			{
				bool ready = GemReady(i);
				MQColor tintCol = ready ? kTintReady : kTintNotReady;
				MQColor gemTint(255, 255, 255, 255);
				if (gem)
				{
					gemTint = gem->SpellGemTintArray[gem->TintIndex];
				}

				ImGui::BeginGroup();
				float posX = ImGui::GetCursorPosX();
				float posY = ImGui::GetCursorPosY();

				ImGui::SetCursorPos(ImVec2(posX + (gemSize.cx - iconPx) * 0.5f, posY + (gemSize.cy - iconPx) * 0.5f));
				m_ctx.Icons->DrawSpellIcon(spell->SpellIcon, CXSize(iconPx, iconPx), tintCol, MQColor(0, 0, 0, 0));

				ImGui::SetCursorPos(ImVec2(posX, posY));
				if (CTextureAnimation* bg = m_ctx.Icons->GemBackground())
				{
					bg->SetCurCell(0);
					imgui::DrawTextureAnimation(bg, gemSize, gemTint, MQColor(0, 0, 0, 0));
				}
				ImGui::EndGroup();

				ImVec2 rectMin = ImGui::GetItemRectMin();
				ImVec2 rectMax = ImGui::GetItemRectMax();

				if (!ready && pLocalPlayer && pDisplay)
				{
					int coolDown = (pLocalPlayer->SpellGemETA[i] - pDisplay->TimeStamp) / 1000;
					if (coolDown > 0 && coolDown < 1801)
					{
						char buf[16];
						sprintf_s(buf, "%d", coolDown);
						ImVec2 textSize = ImGui::CalcTextSize(buf);
						ImVec2 pos((rectMin.x + rectMax.x - textSize.x) * 0.5f,
							(rectMin.y + rectMax.y - textSize.y) * 0.5f);
						ImGui::GetWindowDrawList()->AddText(pos, IM_COL32(0, 230, 230, 255), buf);
					}
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.3f, 1.0f), "%s", spell->Name);
					myui::RenderSpellEffects(spellId);
					ImGui::EndTooltip();
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						DoCommandf("/cast %d", i + 1);
					}
				}
			}

			if (ImGui::BeginPopupContextItem("##GemCtx"))
			{
				if (ImGui::MenuItem(spell ? "Replace Spell..." : "Memorize Spell..."))
				{
					OpenPickerForGem(i);
				}
				if (spell && ImGui::MenuItem("Clear Gem"))
				{
					if (pCastSpellWnd)
					{
						pCastSpellWnd->ForgetMemorizedSpell(i);
					}
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();

			if (!vertical)
			{
				ImGui::SameLine(0.0f, 2.0f);
			}
		}

		DrawOptionsButton();

		ImGui::PopFont();
	}
	ImGui::End();

	if (!w.visible)
	{
		m_ctx.UI->PersistVisibility(GetName());
	}

	m_picker.DrawSpellPicker();
}

void SpellsModule::DrawOptionsButton()
{
	float gemHeight = m_ctx.UI->Num(GetName(), "GemHeight", 32.0f) * m_ctx.UI->Window(GetName()).iconScale;

	ImGui::PushID("OptionsButton");

	if (ImGui::Button(ICON_FA_COG, ImVec2(gemHeight, gemHeight)))
	{
		m_picker.SetOpen(!m_picker.m_pickerOpen);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("Left-click: spell picker");
	}

	ImGui::PopID();
}
