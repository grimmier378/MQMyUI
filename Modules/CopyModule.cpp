#include "CopyModule.h"

#include "../Core/UiConfig.h"
#include "../Core/ActorManager.h"
#include "../Core/Widgets.h"

#include "mq/imgui/ImGuiUtils.h"

#include <map>
#include <set>
#include <utility>

namespace
{
const SettingRow* Find(const std::vector<SettingRow>& rows, const std::string& module, const std::string& name)
{
	for (const SettingRow& r : rows)
	{
		if (r.module == module && r.name == name)
		{
			return &r;
		}
	}
	return nullptr;
}
} // namespace

int CopyModule::PrimaryDst() const
{
	return m_dstSel.empty() ? -1 : *m_dstSel.begin();
}

void CopyModule::RefreshChars()
{
	if (!m_ctx.Settings)
	{
		return;
	}
	m_chars = m_ctx.Settings->GetCharacterList();
	if (m_srcIdx >= (int)m_chars.size())
	{
		m_srcIdx = -1;
	}
	for (auto it = m_dstSel.begin(); it != m_dstSel.end(); )
	{
		if (*it >= (int)m_chars.size())
		{
			it = m_dstSel.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void CopyModule::RefreshRows()
{
	m_src.clear();
	m_dst.clear();
	if (!m_ctx.Settings)
	{
		return;
	}
	if (m_srcIdx >= 0 && m_srcIdx < (int)m_chars.size())
	{
		m_src = m_ctx.Settings->GetAllSettings(m_chars[m_srcIdx].server, m_chars[m_srcIdx].character);
	}
	int primary = PrimaryDst();
	if (primary >= 0 && primary < (int)m_chars.size())
	{
		m_dst = m_ctx.Settings->GetAllSettings(m_chars[primary].server, m_chars[primary].character);
	}
}

void CopyModule::QueueCopy(const std::string& module, const std::string& name, const std::string& type, const std::string& value)
{
	m_queue.push_back(PendingWrite{ module, name, type, value });
}

void CopyModule::RequestApply()
{
	m_pendingDsts.clear();
	for (int idx : m_dstSel)
	{
		if (idx >= 0 && idx < (int)m_chars.size())
		{
			m_pendingDsts.push_back(m_chars[idx]);
		}
	}
	if (m_pendingDsts.empty())
	{
		m_queue.clear();
		return;
	}
	m_applyPending = true;
}

void CopyModule::OnPulse()
{
	if (!m_applyPending)
	{
		return;
	}
	m_applyPending = false;

	if (m_queue.empty() || m_pendingDsts.empty())
	{
		return;
	}

	// Build the changed rows once; the source is always our local DB (mirrored copy).
	std::vector<SettingRow> rows;
	rows.reserve(m_queue.size());
	for (const PendingWrite& write : m_queue)
	{
		rows.push_back(SettingRow{ write.module, write.name, write.type, write.value });
	}
	m_queue.clear();

	auto writeLocal = [&](const CharIdent& dst) {
		if (!m_ctx.Settings)
		{
			return;
		}
		m_ctx.Settings->BeginTransaction();
		for (const SettingRow& r : rows)
		{
			m_ctx.Settings->SetRaw(dst.server, dst.character, r.module, r.name, r.type, r.value);
		}
		m_ctx.Settings->CommitTransaction();
	};

	bool selfTargeted = false;
	for (const CharIdent& dst : m_pendingDsts)
	{
		const bool isSelf = m_ctx.Settings
			&& ci_equals(dst.server, m_ctx.Settings->Server())
			&& ci_equals(dst.character, m_ctx.Settings->Character());

		const myui::PeerRecord* peer = nullptr;
		if (m_ctx.Actors)
		{
			const auto& peers = m_ctx.Actors->Peers();
			auto it = peers.find(myui::PeerKey(dst.server, dst.character));
			if (it != peers.end())
			{
				peer = &it->second;
			}
		}
		const bool online = (peer != nullptr);
		const bool sameStore = peer && m_ctx.Actors && m_ctx.Actors->IsSameStore(*peer);

		if (isSelf)
		{
			writeLocal(dst);
			selfTargeted = true;
		}
		else if (online && !sameStore)
		{
			// Remote store: push rows for them to apply authoritatively; mirror locally for instant feedback.
			writeLocal(dst);
			if (m_ctx.Actors)
			{
				m_ctx.Actors->SendSettingsPush(dst.server, dst.character, rows, false);
			}
		}
		else
		{
			// Same-store online (shared DB) or offline (local mirror): write directly.
			writeLocal(dst);
			if (online && sameStore && m_ctx.Actors)
			{
				m_ctx.Actors->SendCommand(dst.server, dst.character, "ReloadSettings");
			}
		}
	}

	if (selfTargeted && m_ctx.UI)
	{
		m_ctx.UI->Load();
	}

	RefreshRows();
}

void CopyModule::OnRenderGUI()
{
	UiConfig* ui = m_ctx.UI;
	if (!ui)
	{
		return;
	}

	WindowConfig& w = ui->Window(GetName());
	if (!w.visible)
	{
		m_wasOpen = false;
		return;
	}

	if (!m_wasOpen)
	{
		RefreshChars();
		RefreshRows();
		m_wasOpen = true;
	}

	ImGui::SetNextWindowSize(ImVec2(560.0f, 520.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("MyUI Copy Settings##MyUICopy", &w.visible))
	{
		auto charLabel = [&](int idx) -> std::string {
			if (idx < 0 || idx >= (int)m_chars.size())
			{
				return "<none>";
			}
			return m_chars[idx].server + " / " + m_chars[idx].character;
		};

		auto onlineState = [&](int idx, bool& online, bool& sameStore) {
			online = false;
			sameStore = false;
			if (idx < 0 || idx >= (int)m_chars.size() || !m_ctx.Actors)
			{
				return;
			}
			const auto& peers = m_ctx.Actors->Peers();
			auto it = peers.find(myui::PeerKey(m_chars[idx].server, m_chars[idx].character));
			if (it != peers.end())
			{
				online = true;
				sameStore = m_ctx.Actors->IsSameStore(it->second);
			}
		};

		// Render a character entry colored by online state, with an offline tooltip.
		auto charSelectable = [&](int idx, bool selected, ImGuiSelectableFlags flags) -> bool {
			bool online = false, sameStore = false;
			onlineState(idx, online, sameStore);
			std::string label = charLabel(idx) + (online ? " (online)" : " (offline)");
			ImVec4 color = online ? ImVec4(0.45f, 0.9f, 0.45f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			bool clicked = ImGui::Selectable((label + "##c" + std::to_string(idx)).c_str(), selected, flags);
			ImGui::PopStyleColor();
			if (!online && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Offline - copy writes only your local mirror DB; it syncs when this character next comes online.");
			}
			return clicked;
		};

		ImGui::TextWrapped("Source is always your local copy. Online targets receive the changes live and confirm them back; "
			"offline targets are written to your local mirror and sync when they reconnect.");
		ImGui::SameLine();
		mq::imgui::HelpMarker("Characters on the same PC as you (sharing one settings DB) apply instantly with no network traffic. "
			"Characters on another box receive a settings message; offline characters are written to your local mirror only.");

		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo("Source", charLabel(m_srcIdx).c_str()))
		{
			for (int i = 0; i < (int)m_chars.size(); ++i)
			{
				if (charSelectable(i, i == m_srcIdx, ImGuiSelectableFlags_None))
				{
					m_srcIdx = i;
					RefreshRows();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220.0f);
		std::string dstLabel = m_dstSel.empty() ? "<none>" : (std::to_string(m_dstSel.size()) + " selected");
		if (ImGui::BeginCombo("Dest", dstLabel.c_str()))
		{
			for (int i = 0; i < (int)m_chars.size(); ++i)
			{
				if (i == m_srcIdx)
				{
					continue;
				}
				bool sel = m_dstSel.count(i) != 0;
				if (charSelectable(i, sel, ImGuiSelectableFlags_NoAutoClosePopups))
				{
					if (sel)
					{
						m_dstSel.erase(i);
					}
					else
					{
						m_dstSel.insert(i);
					}
					RefreshRows();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		mq::imgui::HelpMarker("Pick one or more destinations. Applying copies to all selected; the diff below compares against the first selected character.");

		const bool ready = (m_srcIdx >= 0 && !m_dstSel.empty());

		ImGui::BeginDisabled(!ready);
		if (myui::StyledButton("Copy All Differences"))
		{
			for (const SettingRow& s : m_src)
			{
				const SettingRow* d = Find(m_dst, s.module, s.name);
				if (!d || d->value != s.value)
				{
					QueueCopy(s.module, s.name, s.type, s.value);
				}
			}
			RequestApply();
			w.visible = false;
		}
		ImGui::EndDisabled();

		ImGui::Separator();

		if (ready && w.visible)
		{
			std::map<std::string, std::set<std::string>> byModule;
			for (const SettingRow& r : m_src)
			{
				byModule[r.module].insert(r.name);
			}
			for (const SettingRow& r : m_dst)
			{
				byModule[r.module].insert(r.name);
			}

			ImGui::BeginChild("##diff");
			for (auto& [module, names] : byModule)
			{
				if (!ImGui::CollapsingHeader(module.c_str()))
				{
					continue;
				}

				if (myui::StyledSmallButton((std::string("Copy section##") + module).c_str()))
				{
					for (const std::string& n : names)
					{
						const SettingRow* s = Find(m_src, module, n);
						if (!s)
						{
							continue;
						}
						const SettingRow* d = Find(m_dst, module, n);
						if (!d || d->value != s->value)
						{
							QueueCopy(module, n, s->type, s->value);
						}
					}
					RequestApply();
				}

				if (ImGui::BeginTable((std::string("##t") + module).c_str(), 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
				{
					for (const std::string& n : names)
					{
						const SettingRow* s = Find(m_src, module, n);
						const SettingRow* d = Find(m_dst, module, n);
						bool differ = (!d) || (!s) || (s->value != d->value);

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(n.c_str());
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(s ? s->value.c_str() : "");
						ImGui::TableNextColumn();
						if (differ)
						{
							ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", d ? d->value.c_str() : "<missing>");
						}
						else
						{
							ImGui::TextUnformatted(d ? d->value.c_str() : "");
						}
						ImGui::TableNextColumn();
						if (s && differ)
						{
							if (myui::StyledSmallButton((std::string("->##") + module + n).c_str()))
							{
								QueueCopy(module, n, s->type, s->value);
								RequestApply();
							}
						}
					}
					ImGui::EndTable();
				}
			}
			ImGui::EndChild();
		}
		else if (!ready)
		{
			ImGui::TextDisabled("Pick two different characters to compare.");
		}
	}
	ImGui::End();

	if (!w.visible)
	{
		ui->PersistVisibility(GetName());
	}
}
