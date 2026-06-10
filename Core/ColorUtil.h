#pragma once

#include <mq/Plugin.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace myui
{
inline MQColor FlashTint(int secondsLeft, int flashThreshold, float pulse)
{
	if (secondsLeft > 0 && secondsLeft < flashThreshold)
	{
		return MQColor(255, 255, 255, static_cast<uint8_t>(pulse * 255.0f));
	}
	return MQColor(255, 255, 255, 255);
}

inline ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t)
{
	t = std::max(0.0f, std::min(1.0f, t));
	return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

inline ImVec4 CalculateColor(const MQColor& minColor, const MQColor& maxColor, int value)
{
	value = std::max(0, std::min(100, value));
	float proportion = static_cast<float>(value) / 100.0f;

	float r = minColor.Red   + proportion * (maxColor.Red   - minColor.Red);
	float g = minColor.Green + proportion * (maxColor.Green - minColor.Green);
	float b = minColor.Blue  + proportion * (maxColor.Blue  - minColor.Blue);
	float a = minColor.Alpha + proportion * (maxColor.Alpha - minColor.Alpha);

	return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline ImVec4 CalculateColor(const MQColor& minColor, const MQColor& maxColor, int value,
	const MQColor& midColor, int midValue)
{
	value = std::max(0, std::min(100, value));
	float r, g, b, a;

	if (value > midValue)
	{
		float proportion = static_cast<float>(value - midValue) / (100 - midValue);
		r = midColor.Red   + proportion * (maxColor.Red   - midColor.Red);
		g = midColor.Green + proportion * (maxColor.Green - midColor.Green);
		b = midColor.Blue  + proportion * (maxColor.Blue  - midColor.Blue);
		a = midColor.Alpha + proportion * (maxColor.Alpha - midColor.Alpha);
	}
	else
	{
		float proportion = static_cast<float>(value) / (midValue == 0 ? 1 : midValue);
		r = minColor.Red   + proportion * (midColor.Red   - minColor.Red);
		g = minColor.Green + proportion * (midColor.Green - minColor.Green);
		b = minColor.Blue  + proportion * (midColor.Blue  - minColor.Blue);
		a = minColor.Alpha + proportion * (midColor.Alpha - minColor.Alpha);
	}

	return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline void DrawBar(const char* label, int current, int maximum, float height,
	const MQColor& minColor, const MQColor& maxColor, const char* tooltip)
{
	if (maximum <= 0)
	{
		maximum = 1;
	}

	float percentage = static_cast<float>(current) / maximum;
	int percentageInt = static_cast<int>(percentage * 100);
	ImVec4 color = CalculateColor(minColor, maxColor, percentageInt);

	ImVec2 p = ImGui::GetCursorScreenPos();
	float w = ImGui::GetContentRegionAvail().x;

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
	ImGui::ProgressBar(percentage, ImVec2(w, height), "");
	ImGui::PopStyleColor();

	char buf[32];
	snprintf(buf, sizeof(buf), "%d / %d", current, maximum);
	ImVec2 ts = ImGui::CalcTextSize(buf);
	ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (height - ts.y) * 0.5f),
		ImGui::GetColorU32(ImGuiCol_Text), buf);

	if (tooltip && ImGui::IsItemHovered())
	{
		ImGui::SetItemTooltip("%s: %d / %d", tooltip, current, maximum);
	}
}

inline void DrawBarPct(const char* id, int pct, float height,
	const MQColor& minColor, const MQColor& maxColor, const char* overlay)
{
	pct = std::max(0, std::min(100, pct));
	ImVec4 color = CalculateColor(minColor, maxColor, pct);

	ImVec2 p = ImGui::GetCursorScreenPos();
	float w = ImGui::GetContentRegionAvail().x;

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
	ImGui::ProgressBar(pct / 100.0f, ImVec2(w, height), "");
	ImGui::PopStyleColor();

	if (overlay && overlay[0])
	{
		ImVec2 ts = ImGui::CalcTextSize(overlay);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (height - ts.y) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text), overlay);
	}
}

inline void DrawVertBar(int pct, float width, float height, const MQColor& minColor, const MQColor& maxColor)
{
	pct = std::max(0, std::min(100, pct));
	ImVec4 color = CalculateColor(minColor, maxColor, pct);

	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
	float fill = height * (pct / 100.0f);
	dl->AddRectFilled(ImVec2(p.x, p.y + height - fill), ImVec2(p.x + width, p.y + height), ImGui::GetColorU32(color), 3.0f);
	ImGui::Dummy(ImVec2(width, height));
}
} // namespace myui
