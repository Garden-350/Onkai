#include "AttackOrbs.h"
#include "RibbonShapes.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void AttackOrbs::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(kSegments + 1);
	m_Orbs.reserve(kMaxOrbs);
	m_Rng.seed(std::random_device{}());
}

float AttackOrbs::Rand(float a, float b)
{
	std::uniform_real_distribution<float> d(a, b);
	return d(m_Rng);
}

// 3次ベジェ曲線の上の点（s : 0 = 判定円、1 = 敵）
Vector3 AttackOrbs::Bezier(const Orb& o, float s)
{
	float u = 1.0f - s;
	return o.p0 * (u * u * u) + o.p1 * (3.0f * u * u * s) + o.p2 * (3.0f * u * s * s) + o.p3 * (s * s * s);
}

void AttackOrbs::Launch(int lane, const Vector3& from, const Vector3& to, const Vector3& color, bool critical)
{
	if ((int)m_Orbs.size() >= kMaxOrbs) m_Orbs.erase(m_Orbs.begin());	// 一番古いものを消す

	// 左手のレーンは左へ、右手のレーンは右へ。外側のレーンほど大きくふくらむ
	float side = (lane < 3) ? -1.0f : 1.0f;
	float outer = 0.6f + 0.4f * std::fabs(lane - 2.5f) / 2.5f;
	float j = Rand(0.85f, 1.15f);	// 毎回少しだけ形を変える

	Orb o;
	o.lane = lane;
	o.p0 = from;
	o.p1 = from + Vector3(side * kSideOut * outer * j, kArcHeight * j, 0.0f);			// 判定円の真上・外側へ
	o.p2 = to + Vector3(side * kSideOut * 0.35f * outer, kArcHeight * 0.45f * j, 0.0f);	// 敵の少し上・外側から入る
	o.p3 = to;
	o.color = color;
	o.t = 0.0f;
	o.dur = kFlightTime * Rand(0.9f, 1.1f);
	o.crit = critical;
	m_Orbs.push_back(o);
}

void AttackOrbs::Update(float deltaTime)
{
	m_Arrived.clear();
	for (auto& o : m_Orbs)
	{
		o.t += deltaTime;
		if (o.t >= o.dur) m_Arrived.push_back(o);
	}
	m_Orbs.erase(std::remove_if(m_Orbs.begin(), m_Orbs.end(),
		[](const Orb& o) { return o.t >= o.dur; }), m_Orbs.end());

	// 着いたものを知らせる（ループの外で呼ぶので、呼んだ先で Launch しても大丈夫）
	if (m_OnArrive)
	{
		for (const auto& o : m_Arrived) m_OnArrive(o.lane, o.p3, o.color, o.crit);
	}
}

void AttackOrbs::Draw(Camera* cam)
{
	if (m_Orbs.empty()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);

	const Vector3 gold(1.0f, 0.85f, 0.4f);
	for (const auto& o : m_Orbs)
	{
		float k = std::clamp(o.t / o.dur, 0.0f, 1.0f);
		float head = k * (0.55f + 0.45f * k);		// 少しずつ速くなって、勢いよく当たる
		float tail = std::max(0.0f, head - kTrail);
		if (head - tail < 0.002f) continue;

		m_Pts.clear();
		for (int i = 0; i <= kSegments; i++)
		{
			float r = i / (float)kSegments;
			LongBeamRibbon::Point p;
			p.pos = Bezier(o, tail + (head - tail) * r);
			p.color = RibbonShapes::MakeColor(o.color, 0.1f + 0.9f * r);	// 尾は薄く、先は濃く
			m_Pts.push_back(p);
		}

		float width = kWidth * (o.crit ? kCritWidthMul : 1.0f);
		float intensity = o.crit ? 1.8f : 1.2f;
		Vector3 glow = o.crit ? gold : o.color;	// 弱点へ飛ぶ光は外側が金色
		m_Ribbon.Draw(cam, m_Pts, width, intensity, -o.t * 3.0f, glow);
	}

	Renderer::SetBlendState(BS_NONE);
}
