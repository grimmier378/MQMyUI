#include "BarEngine.h"
#include "UiConfig.h"
#include "ColorUtil.h"

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

void DrawSlicedFill(ImDrawList* dl, ImVec2 p0, ImVec2 p1, const ImVec4& lo, const ImVec4& hi,
	int dir, float rounding)
{
	float w = p1.x - p0.x;
	float h = p1.y - p0.y;
	if (w <= 0.0f || h <= 0.0f)
	{
		return;
	}

	const float step = 2.0f;
	bool gridX = (dir != 1);
	bool gridY = (dir == 1 || dir == 2 || dir == 3);

	int nx = gridX ? std::max(1, (int)ceilf(w / step)) : 1;
	int ny = gridY ? std::max(1, (int)ceilf(h / step)) : 1;

	for (int ix = 0; ix < nx; ++ix)
	{
		float cx0 = p0.x + (w * ix) / nx;
		float cx1 = p0.x + (w * (ix + 1)) / nx;
		float fx = (nx > 1) ? ((ix + 0.5f) / nx) : 0.0f;
		for (int iy = 0; iy < ny; ++iy)
		{
			float cy0 = p0.y + (h * iy) / ny;
			float cy1 = p0.y + (h * (iy + 1)) / ny;
			float fy = (ny > 1) ? ((iy + 0.5f) / ny) : 0.0f;

			float frac;
			switch (dir)
			{
			case 1:  frac = fy; break;
			case 2:  frac = (fx + fy) * 0.5f; break;
			case 3:  frac = ((1.0f - fx) + fy) * 0.5f; break;
			default: frac = fx; break;
			}

			dl->PushClipRect(ImVec2(cx0, cy0), ImVec2(cx1, cy1), true);
			dl->AddRectFilled(p0, p1, ImGui::GetColorU32(LerpV4(lo, hi, frac)), rounding);
			dl->PopClipRect();
		}
	}
}

template <class ColorFn>
void DrawSlicedBand(ImDrawList* dl, ImVec2 f0, ImVec2 f1, bool vertical, float rounding, ColorFn colorAt)
{
	float w = f1.x - f0.x;
	float h = f1.y - f0.y;
	if (w <= 0.0f || h <= 0.0f)
	{
		return;
	}

	const float step = 2.0f;
	if (vertical)
	{
		int ny = std::max(1, (int)ceilf(h / step));
		for (int iy = 0; iy < ny; ++iy)
		{
			float cy0 = f0.y + (h * iy) / ny;
			float cy1 = f0.y + (h * (iy + 1)) / ny;
			float t = (iy + 0.5f) / ny;
			dl->PushClipRect(ImVec2(f0.x, cy0), ImVec2(f1.x, cy1), true);
			dl->AddRectFilled(f0, f1, colorAt(t), rounding);
			dl->PopClipRect();
		}
	}
	else
	{
		int nx = std::max(1, (int)ceilf(w / step));
		for (int ix = 0; ix < nx; ++ix)
		{
			float cx0 = f0.x + (w * ix) / nx;
			float cx1 = f0.x + (w * (ix + 1)) / nx;
			float t = (ix + 0.5f) / nx;
			dl->PushClipRect(ImVec2(cx0, f0.y), ImVec2(cx1, f1.y), true);
			dl->AddRectFilled(f0, f1, colorAt(t), rounding);
			dl->PopClipRect();
		}
	}
}
} // namespace

void DrawStyledBar(const char* id, float pct, const BarStyle& style, int curValue, int maxValue)
{
	pct = std::max(0.0f, std::min(100.0f, pct));
	float target = pct / 100.0f;

	ImGuiID gid = ImGui::GetID(id);
	float p = Tween(gid, target, style.tweenSeconds);

	ImVec2 avail = ImGui::GetContentRegionAvail();
	float barW, barH;
	if (style.vertical)
	{
		barW = style.width > 3.0f ? style.width : 24.0f;
		barH = style.height > 3.0f ? style.height : barW;
	}
	else
	{
		barW = style.width > 3.0f ? style.width : avail.x;
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
	ImVec2 p0 = pos;
	ImVec2 p1 = ImVec2(pos.x + barW, pos.y + barH);

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
			DrawSlicedFill(dl, f0, f1, lo, endHi, style.gradientDir, rounding);
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
		for (float t = style.tickEvery; t < 1.0f; t += style.tickEvery)
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
			if (style.vertical)
			{
				float top = f0.y;
				DrawSlicedBand(dl, f0, f1, true, rounding, [&](float t) {
					float y = f0.y + t * (f1.y - f0.y);
					float d = y - top;
					float a = (d < gt) ? (1.0f - d / gt) * glowA : 0.0f;
					return ImGui::GetColorU32(ImVec4(1, 1, 1, a));
				});
			}
			else
			{
				float right = f1.x;
				DrawSlicedBand(dl, f0, f1, false, rounding, [&](float t) {
					float x = f0.x + t * (f1.x - f0.x);
					float d = right - x;
					float a = (d < gt) ? (1.0f - d / gt) * glowA : 0.0f;
					return ImGui::GetColorU32(ImVec4(1, 1, 1, a));
				});
			}
		}
	}

	if (style.shimmerOn && p > 0.001f)
	{
		float phase = fmodf((float)ImGui::GetTime() * style.shimmerSpeed, 1.0f);
		float hw = style.shimmerWidth * 0.5f;
		const float midA = 0.25f;
		if (hw > 0.0f)
		{
			if (style.vertical)
			{
				float yc = p1.y - barH * phase;
				DrawSlicedBand(dl, f0, f1, true, rounding, [&](float t) {
					float y = f0.y + t * (f1.y - f0.y);
					float dist = fabsf(y - yc);
					float a = (dist < hw) ? midA * (1.0f - dist / hw) : 0.0f;
					return ImGui::GetColorU32(ImVec4(1, 1, 1, a));
				});
			}
			else
			{
				float xc = p0.x + barW * phase;
				DrawSlicedBand(dl, f0, f1, false, rounding, [&](float t) {
					float x = f0.x + t * (f1.x - f0.x);
					float dist = fabsf(x - xc);
					float a = (dist < hw) ? midA * (1.0f - dist / hw) : 0.0f;
					return ImGui::GetColorU32(ImVec4(1, 1, 1, a));
				});
			}
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
		dl->AddText(ImVec2(p0.x + (barW - ts.x) * 0.5f, p0.y + (barH - ts.y) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text), buf);
		ImGui::PopFont();
	}

	ImGui::Dummy(ImVec2(barW, barH));
}
} // namespace myui
