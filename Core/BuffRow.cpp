#include "BuffRow.h"

#include "IconHelper.h"
#include "ColorUtil.h"

#include <mq/Plugin.h>

#include <cstdio>

namespace myui
{
void DrawBuffIconLineup(const char* idKey, const std::vector<BuffInfo>& buffs,
	const BuffIconLineupOpts& opts, IconHelper* icons)
{
	if (!icons)
	{
		return;
	}

	ImGui::PushID(idKey);

	int last = static_cast<int>(buffs.size()) - 1;
	if (opts.trimTrailingEmpty)
	{
		last = -1;
		for (int k = 0; k < static_cast<int>(buffs.size()); ++k)
		{
			if (!buffs[k].isEmpty)
			{
				last = k;
			}
		}
	}

	float avail = ImGui::GetContentRegionAvail().x;
	float lineWidth = 0.0f;

	for (int k = 0; k <= last; ++k)
	{
		const BuffInfo& buff = buffs[k];
		ImGui::PushID(buff.slot);

		if (buff.isEmpty)
		{
			icons->DrawEmptySlot(CXSize(opts.iconSize, opts.iconSize));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetItemTooltip("Slot %d (Empty)", buff.slot + 1);
			}
		}
		else
		{
			MQColor border = BuffBorderColor(buff.beneficial, opts.showCaster ? buff.caster.c_str() : nullptr, opts.selfCast);

			int secondsLeft = buff.durationMs / 1000;
			MQColor tint = FlashTint(secondsLeft, opts.flashThreshold, opts.pulse);

			icons->DrawSpellIcon(buff.iconId, CXSize(opts.iconSize, opts.iconSize), tint, border);

			if (ImGui::IsItemHovered())
			{
				char timeLabel[32];
				FormatDuration(timeLabel, sizeof(timeLabel), buff.durationMs, opts.withHours);
				ImGui::BeginTooltip();
				ImGui::Text("%s", buff.name.c_str());
				if (buff.durationMs != 0)
				{
					ImGui::Text("Time: %s", timeLabel);
				}
				if (opts.showCaster && !buff.caster.empty())
				{
					ImGui::Text("Caster: %s", mq::IsAnonymized() ? "Player" : buff.caster.c_str());
				}
				ImGui::EndTooltip();
			}

			if (opts.contextMenu)
			{
				opts.contextMenu(buff);
			}
		}

		ImGui::PopID();

		lineWidth += opts.iconSize + 2.0f;
		if (lineWidth < avail - opts.iconSize)
		{
			ImGui::SameLine(0.0f, 2.0f);
		}
		else
		{
			lineWidth = 0.0f;
		}
	}

	ImGui::PopID();
}
} // namespace myui
