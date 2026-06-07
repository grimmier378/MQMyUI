#include "BarEngine.h"
#include "UiConfig.h"
#include "ColorUtil.h"

#include "mq/imgui/AlphaMask.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace myui
{
namespace
{
ImVec4 ToV4(const MQColor& c)
{
	return ImVec4(c.Red / 255.0f, c.Green / 255.0f, c.Blue / 255.0f, c.Alpha / 255.0f);
}

ImVec4 LerpV4(const ImVec4& a, const ImVec4& b, float t)
{
	t = std::max(0.0f, std::min(1.0f, t));
	return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

float Tween(ImGuiID id, float target, float seconds)
{
	static std::unordered_map<ImGuiID, float> s_state;
	auto it = s_state.find(id);
	if (it == s_state.end())
	{
		s_state[id] = target;
		return target;
	}
	float cur = it->second;
	if (seconds <= 0.0f)
	{
		it->second = target;
		return target;
	}
	float dt = ImGui::GetIO().DeltaTime;
	float k = 1.0f - expf(-dt / std::max(0.0001f, seconds * 0.4f));
	cur += (target - cur) * k;
	if (fabsf(target - cur) < 0.0005f)
	{
		cur = target;
	}
	it->second = cur;
	return cur;
}

bool TrackDescending(ImGuiID id, float p)
{
	static std::unordered_map<ImGuiID, float> s_prev;
	static std::unordered_map<ImGuiID, bool> s_desc;
	bool desc = s_desc.count(id) ? s_desc[id] : false;
	auto it = s_prev.find(id);
	if (it != s_prev.end())
	{
		float dp = p - it->second;
		if (dp < -1e-4f)
		{
			desc = true;
		}
		else if (dp > 1e-4f)
		{
			desc = false;
		}
	}
	s_prev[id] = p;
	s_desc[id] = desc;
	return desc;
}

template <class DrawFn>
void DrawMaskedFill(ImDrawList* dl, ImVec2 f0, ImVec2 f1, float rounding, DrawFn drawContent)
{
	if (f1.x - f0.x <= 0.0f || f1.y - f0.y <= 0.0f)
	{
		return;
	}
	if (rounding <= 0.0f)
	{
		dl->PushClipRect(f0, f1, true);
		drawContent();
		dl->PopClipRect();
		return;
	}
	mq::imgui::CreateCoverageMaskLayer(dl, f0, f1);
	dl->AddRectFilled(f0, f1, IM_COL32_WHITE, rounding);
	mq::imgui::BeginCoverageMaskedDraw(dl);
	drawContent();
	mq::imgui::EndCoverageMaskedDraw(dl);
}

template <class DrawFn>
void DrawStencilMaskedFill(ImDrawList* dl, ImVec2 f0, ImVec2 f1, float rounding, DrawFn drawContent)
{
	if (f1.x - f0.x <= 0.0f || f1.y - f0.y <= 0.0f)
	{
		return;
	}
	if (rounding <= 0.0f)
	{
		dl->PushClipRect(f0, f1, true);
		drawContent();
		dl->PopClipRect();
		return;
	}
	mq::imgui::CreateMaskLayer(dl);
	dl->AddRectFilled(f0, f1, IM_COL32_WHITE, rounding);
	mq::imgui::BeginMaskedDraw(dl, mq::imgui::AlphaMaskOp::Union);
	drawContent();
	mq::imgui::EndMaskedDraw(dl);
}

void DrawGradientFill(ImDrawList* dl, ImVec2 f0, ImVec2 f1, const ImVec4& lo, const ImVec4& hi,
	int dir, float rounding)
{
	ImU32 cLo = ImGui::GetColorU32(lo);
	ImU32 cHi = ImGui::GetColorU32(hi);
	ImU32 cMid = ImGui::GetColorU32(LerpV4(lo, hi, 0.5f));

	ImU32 ul, ur, br, bl;
	switch (dir)
	{
	case 1:
		ul = cLo; ur = cLo; br = cHi; bl = cHi;
		break;
	case 2:
		ul = cMid; ur = cLo; br = cMid; bl = cHi;
		break;
	case 3:
		ul = cLo; ur = cMid; br = cHi; bl = cMid;
		break;
	default:
		ul = cLo; ur = cHi; br = cHi; bl = cLo;
		break;
	}

	DrawMaskedFill(dl, f0, f1, rounding, [&]() {
		dl->AddRectFilledMultiColor(f0, f1, ul, ur, br, bl);
	});
}
} // namespace

void DrawStyledBarRect(const char* id, float pct, const BarStyle& style, const ImVec2& p0, const ImVec2& p1,
	int curValue, int maxValue)
{
	pct = std::max(0.0f, std::min(100.0f, pct));
	float target = pct / 100.0f;

	ImGuiID gid = ImGui::GetID(id);
	float p = Tween(gid, target, style.tweenSeconds);
	bool descending = TrackDescending(gid, p);

	float barW = p1.x - p0.x;
	float barH = p1.y - p0.y;
	if (barW <= 0.0f)
	{
		barW = 1.0f;
	}
	if (barH <= 0.0f)
	{
		barH = 1.0f;
	}

	float maxRound = (style.vertical ? barW : barH) * 0.5f;
	float rounding = std::min(style.rounding, maxRound);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	dl->AddRectFilled(p0, p1, ImGui::GetColorU32(ToV4(style.bgColor)), rounding);

	ImVec2 f0, f1;
	if (style.vertical)
	{
		f0 = ImVec2(p0.x, p1.y - barH * p);
		f1 = p1;
	}
	else
	{
		f0 = p0;
		f1 = ImVec2(p0.x + barW * p, p1.y);
	}

	if (p > 0.0001f)
	{
		ImVec4 lo = ToV4(style.fillLow);
		ImVec4 hi = ToV4(style.fillHigh);
		if (style.gradientOn)
		{
			ImVec4 endHi = (style.gradientMode == 1) ? LerpV4(lo, hi, p) : hi;
			DrawGradientFill(dl, f0, f1, lo, endHi, style.gradientDir, rounding);
		}
		else
		{
			ImVec4 solid = CalculateColor(style.fillLow, style.fillHigh, (int)(p * 100.0f));
			dl->AddRectFilled(f0, f1, ImGui::GetColorU32(solid), rounding);
		}
	}

	if (style.ticksOn && style.tickEvery > 0.0f)
	{
		ImU32 tcol = ImGui::GetColorU32(ImVec4(1, 1, 1, style.tickAlpha / 255.0f));
		for (float t = style.tickEvery; t < 1.0f - style.tickEvery * 0.5f; t += style.tickEvery)
		{
			if (style.vertical)
			{
				float y = p1.y - barH * t;
				dl->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), tcol, style.tickThickness);
			}
			else
			{
				float x = p0.x + barW * t;
				dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), tcol, style.tickThickness);
			}
		}
	}

	if (style.glowOn && p > 0.001f)
	{
		float gt = style.glowThickness > 0.0f ? style.glowThickness : (style.vertical ? barH : barW) * 0.2f;
		float glowA = style.glowAlpha / 255.0f;
		if (gt > 0.0f)
		{
			DrawStencilMaskedFill(dl, f0, f1, rounding, [&]() {
				ImU32 cEdge = ImGui::GetColorU32(ImVec4(1, 1, 1, glowA));
				if (style.vertical)
				{
					float span = std::min(gt, f1.y - f0.y);
					ImU32 cFar = ImGui::GetColorU32(ImVec4(1, 1, 1, (1.0f - span / gt) * glowA));
					dl->AddRectFilledMultiColor(ImVec2(f0.x, f0.y), ImVec2(f1.x, f0.y + span),
						cEdge, cEdge, cFar, cFar);
				}
				else
				{
					float span = std::min(gt, f1.x - f0.x);
					ImU32 cFar = ImGui::GetColorU32(ImVec4(1, 1, 1, (1.0f - span / gt) * glowA));
					dl->AddRectFilledMultiColor(ImVec2(f1.x - span, f0.y), ImVec2(f1.x, f1.y),
						cFar, cEdge, cEdge, cFar);
				}
			});
		}
	}

	if (style.shimmerOn && p > 0.001f)
	{
		float phase = fmodf((float)ImGui::GetTime() * style.shimmerSpeed, 1.0f);
		if (style.shimmerFollows && descending)
		{
			phase = 1.0f - phase;
		}
		float hw = style.shimmerWidth * 0.5f;
		const float midA = 0.25f;
		if (hw > 0.0f)
		{
			ImU32 cPeak = ImGui::GetColorU32(ImVec4(1, 1, 1, midA));
			ImU32 cZero = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.0f));
			DrawStencilMaskedFill(dl, f0, f1, rounding, [&]() {
				if (style.vertical)
				{
					float yc = p1.y - barH * phase;
					dl->AddRectFilledMultiColor(ImVec2(f0.x, yc - hw), ImVec2(f1.x, yc),
						cZero, cZero, cPeak, cPeak);
					dl->AddRectFilledMultiColor(ImVec2(f0.x, yc), ImVec2(f1.x, yc + hw),
						cPeak, cPeak, cZero, cZero);
				}
				else
				{
					float xc = p0.x + barW * phase;
					dl->AddRectFilledMultiColor(ImVec2(xc - hw, f0.y), ImVec2(xc, f1.y),
						cZero, cPeak, cPeak, cZero);
					dl->AddRectFilledMultiColor(ImVec2(xc, f0.y), ImVec2(xc + hw, f1.y),
						cPeak, cZero, cZero, cPeak);
				}
			});
		}
	}

	if (style.border)
	{
		dl->AddRect(p0, p1, ImGui::GetColorU32(ToV4(style.borderColor)), rounding, 0, style.borderThickness);
	}

	if (style.textMode != 0)
	{
		char buf[48] = { 0 };
		if (style.textMode == 2 && curValue >= 0 && maxValue >= 0)
		{
			snprintf(buf, sizeof(buf), "%d / %d", curValue, maxValue);
		}
		else
		{
			snprintf(buf, sizeof(buf), style.textFormat.empty() ? "%.0f%%" : style.textFormat.c_str(), pct);
		}

		ImGui::PushFont(nullptr, ImGui::GetFontSize() * (style.textScale > 0.0f ? style.textScale : 1.0f));
		ImVec2 ts = ImGui::CalcTextSize(buf);
		ImVec2 textPos(p0.x + (barW - ts.x) * 0.5f, p0.y + (barH - ts.y) * 0.5f);
		if (style.textDropShadow)
		{
			dl->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 160), buf);
		}
		dl->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), buf);
		ImGui::PopFont();
	}
}

void DrawStyledBar(const char* id, float pct, const BarStyle& style, int curValue, int maxValue)
{
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float barW, barH;
	if (style.vertical)
	{
		barW = style.width > 3.0f ? style.width : 24.0f;
		barH = style.height > 3.0f ? style.height : barW;
	}
	else
	{
		barW = avail.x;
		barH = style.height;
	}
	if (barW <= 0.0f)
	{
		barW = 1.0f;
	}
	if (barH <= 0.0f)
	{
		barH = 1.0f;
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	DrawStyledBarRect(id, pct, style, pos, ImVec2(pos.x + barW, pos.y + barH), curValue, maxValue);

	ImGui::Dummy(ImVec2(barW, barH));
}
} // namespace myui
