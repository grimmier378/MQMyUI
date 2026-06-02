#include "Widgets.h"

#include <cmath>
#include <cstring>

namespace myui
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;

void RenderStar(ImDrawList* dl, ImVec2 center, float radius, float rot, ImU32 col)
{
	const int points = 5;
	ImVec2 pts[10];
	for (int i = 0; i < points * 2; ++i)
	{
		float r = (i & 1) ? radius * 0.45f : radius;
		float a = rot + (float)i * (kPi / points) - kPi * 0.5f;
		pts[i] = ImVec2(center.x + cosf(a) * r, center.y + sinf(a) * r);
	}
	for (int i = 0; i < points * 2; ++i)
	{
		dl->AddTriangleFilled(center, pts[i], pts[(i + 1) % (points * 2)], col);
	}
}

void RenderMoon(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, ImU32 carve)
{
	dl->AddCircleFilled(center, radius, col, 16);
	dl->AddCircleFilled(ImVec2(center.x + radius * 0.45f, center.y - radius * 0.2f), radius * 0.9f, carve, 16);
}

int g_globalToggleFlags = kDefaultToggleFlags;
ImVec2 g_globalToggleSize = ImVec2(0.0f, 0.0f);
} // namespace

void SetGlobalToggleStyle(int flags, ImVec2 size)
{
	g_globalToggleFlags = flags;
	g_globalToggleSize = size;
}

bool DrawToggle(const char* id, bool* value, int flags, ImVec2 size)
{
	if (!value)
	{
		return false;
	}

	if (flags < 0)
	{
		flags = g_globalToggleFlags;
	}
	if (size.x <= 0.0f && size.y <= 0.0f)
	{
		size = g_globalToggleSize;
	}

	char label[128] = { 0 };
	const char* sep = strstr(id, "##");
	size_t labelLen = sep ? (size_t)(sep - id) : strlen(id);
	if (labelLen >= sizeof(label))
	{
		labelLen = sizeof(label) - 1;
	}
	memcpy(label, id, labelLen);
	label[labelLen] = 0;

	float height = size.y > 0.0f ? size.y : ImGui::GetFrameHeight() * 0.85f;
	float width  = size.x > 0.0f ? size.x : height * 1.9f;
	float radius = height * 0.5f;

	const bool rightLabel = (flags & ToggleFlags_RightLabel) != 0;
	const bool hasLabel   = label[0] != 0;

	ImGui::BeginGroup();
	ImGui::PushID(id);

	if (hasLabel && !rightLabel)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	float yOff = (ImGui::GetFrameHeight() - height) * 0.5f;
	pos.y += yOff > 0.0f ? yOff : 0.0f;

	ImGui::InvisibleButton("##knob", ImVec2(width, height));
	bool clicked = ImGui::IsItemClicked();
	bool hovered = ImGui::IsItemHovered();
	if (clicked)
	{
		*value = !*value;
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec4 offCol = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
	ImVec4 onCol  = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);

	dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
		ImGui::GetColorU32(*value ? onCol : offCol), radius);

	float t = *value ? 1.0f : 0.0f;
	ImVec2 knob(pos.x + radius + t * (width - height), pos.y + radius);

	const float time = (float)ImGui::GetTime();
	bool animate = (flags & ToggleFlags_AnimateKnob) || ((flags & ToggleFlags_AnimateOnHover) && hovered);

	ImVec4 knobCol(1.0f, 1.0f, 1.0f, 1.0f);
	if ((flags & ToggleFlags_PulseOnHover) && hovered)
	{
		float pulse = 0.6f + 0.4f * sinf(time * 4.0f);
		knobCol = ImVec4(pulse, pulse, pulse, 1.0f);
	}
	ImU32 knobU32 = ImGui::GetColorU32(knobCol);
	ImU32 trackU32 = ImGui::GetColorU32(*value ? onCol : offCol);

	if ((flags & ToggleFlags_GlowOnHover) && hovered)
	{
		float breathe = 0.25f + 0.2f * sinf(time * 3.0f);
		for (int i = 3; i >= 1; --i)
		{
			ImVec4 g = *value ? onCol : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
			g.w = breathe / (float)i;
			dl->AddCircleFilled(knob, radius * (0.8f + 0.35f * i), ImGui::GetColorU32(g), 20);
		}
	}

	if (flags & ToggleFlags_StarKnob)
	{
		if (*value)
		{
			RenderStar(dl, knob, radius * 0.8f, animate ? time * 1.5f : 0.0f, knobU32);
		}
		else
		{
			RenderMoon(dl, knob, radius * 0.7f, knobU32, trackU32);
		}
	}
	else
	{
		dl->AddCircleFilled(knob, radius * 0.8f, knobU32, 20);
		if (flags & ToggleFlags_KnobBorder)
		{
			dl->AddCircle(knob, radius * 0.8f, ImGui::GetColorU32(ImVec4(0, 0, 0, 0.6f)), 20, 1.0f);
		}
	}

	if (hasLabel && rightLabel)
	{
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
	}

	ImGui::PopID();
	ImGui::EndGroup();
	return clicked;
}
} // namespace myui
