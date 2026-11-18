#include "Gimmick.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

namespace
{
	constexpr float kStep = 1.0f / 240.0f;	//速さの表の細かさ

	std::vector<GimmickEvent> s_Events;
	std::vector<float> s_Scroll;		//s_Scroll[i] = Scroll(i*kStep)

	struct Peak { float time; float scroll; };	//進んだ量が一番大きくなるところ。逆走が始まるところでも
	std::vector<Peak> s_Peaks;

	float Smooth(float x)
	{
		x = std::clamp(x, 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f * x);
	}
}

float GimmickEvent::Weight(float t) const
{
	if (t <= start || t >= end) return 0.0f;
	float in = (easeIn > 0.0f) ? Smooth((t - start) / easeIn) : 1.0f;
	float out = (easeOut > 0.0f) ? Smooth((end - t) / easeOut) : 1.0f;
	return std::min(in, out);
}

namespace Gimmick
{
	float SpeedAt(float t)
	{
		float v = 1.0f;
		for (const auto& e : s_Events)
		{
			if (e.type != GimmickType::Speed) continue;
			v *= 1.0f + (e.speed - 1.0f) * e.Weight(t);	//1からe.speedへなめらか
		}
		return v;
	}

	void Load(const std::vector<GimmickEvent>& events, float songLength)
	{
		s_Events = events;
		s_Scroll.clear();
		s_Peaks.clear();

		bool any = false;
		for (const auto& e : s_Events) if (e.type == GimmickType::Speed) any = true;
		if (!any) return;	//速さの演出がなければ表はいらない(Scroll(t) = t)

		if (songLength < 1.0f) songLength = 1.0f;
		int n = (int)(songLength / kStep) + 2;
		s_Scroll.assign(n, 0.0f);
		double s = 0.0;
		for (int i = 1; i < n; i++)
		{
			s += (double)SpeedAt((i - 0.5f) * kStep) * kStep;	//1マスの真ん中の速さ＊1マスの長さ
			s_Scroll[i] = (float)s;
		}

		//逆走が始まるところ(進んだ量が増える->減るに変わるところ)を覚えておく
		for (int i = 1; i + 1 < n; i++)
		{
			if (s_Scroll[i] > s_Scroll[i - 1] && s_Scroll[i] >= s_Scroll[i + 1] 
				&& s_Scroll[i + 1] < s_Scroll[i] - 0.00001f)
			{
				s_Peaks.push_back({ i * kStep,s_Scroll[i] });
			}
		}
	}

	void Clear()
	{
		s_Events.clear();
		s_Scroll.clear();
		s_Peaks.clear();
	}

	float Scroll(float t)
	{
		if (s_Scroll.empty() || t <= 0.0f) return t;	//演出なし
		float f = t / kStep;
		int i = (int)f;
		if (i + 1 >= (int)s_Scroll.size())
		{
			//表の後ろは普通の速さで
			return s_Scroll.back() + (t - (float)(s_Scroll.size() - 1) * kStep);
		}
		return s_Scroll[i] + (s_Scroll[i + 1] - s_Scroll[i]) * (f - (float)i);
	}

	float Progress(float spawnTime, float arriveTime, float now)
	{
		float duration = arriveTime - spawnTime;
		if (duration < 0.0001f)duration = 0.0001f;
		if (s_Scroll.empty()) return (now - spawnTime) / duration;
		return 1.0f - (Scroll(arriveTime) - Scroll(now)) / duration;
	}

	bool PointVisible(float tau, float now, float approach)
	{
		if (approach < 0.0001f) approach = 0.0001f;
		float sTau = Scroll(tau);
		float frac = (sTau - Scroll(now)) / approach;	//0=判定円 1=奥の端 マイナス=判定円より手前
		if (frac > 1.0f || frac < -kBelowLimit) return false;
		if (frac < 0.0f) return SpeedAt(now) < 0.0f;	//手前は、逆走で上がってきている間だけ
		for (const auto& pk : s_Peaks)					//この先の逆走で、一度判定円を通り過ぎてしまうなら隠す
			if (pk.time > now && pk.time < tau && pk.scroll > sTau) return false;
		return true;
	}

	bool InView(float spawnTime, float arriveTime, float now)
	{
		if (now > arriveTime) return false;	//判定の時刻を過ぎたら今まで通り描かない
		if (s_Scroll.empty()) return now >= spawnTime;	//演出なし：今までと同じ
		return PointVisible(arriveTime, now, arriveTime - spawnTime);
	}

	bool HasSpeed() { return !s_Scroll.empty(); }

	const std::vector<GimmickEvent>& Events() { return s_Events; }
}

