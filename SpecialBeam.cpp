#include "SpecialBeam.h"
#include "RibbonShapes.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	// 群れの光の色（ここに足すと色が増える）
	const Vector3 kSwarmPalette[] =
	{
		Vector3(1.0f, 0.85f, 0.40f),	//金
		Vector3(1.0f, 0.50f, 0.15f),	//オレンジ
		Vector3(1.0f, 0.30f, 0.75f),	//マゼンタ
		Vector3(0.35f, 0.90f, 1.0f),	//水色
		Vector3(0.60f, 0.40f, 1.0f),	//紫
		Vector3(1.0f, 1.0f, 1.0f),		//白
	};
	constexpr int kPaletteCount = sizeof(kSwarmPalette) / sizeof(kSwarmPalette[0]);
}

void SpecialBeam::Init()
{
	m_Ribbon.Init();
	m_Points.reserve(kMainSegments + 1);
	m_Strands.reserve(kSwarmCount);
	m_Rng.seed(std::random_device{}());
	m_Active = false;
}

float SpecialBeam::Rand(float a, float b)
{
	std::uniform_real_distribution<float> d(a, b);
	return d(m_Rng);
}

//3次ベジェの上の点
Vector3 SpecialBeam::Cubic(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, float s)
{
	float u = 1.0f - s;
	return a * (u * u * u) + b * (3.0f * u * u * s) + c * (3.0f * u * s * s) + d * (s * s * s);
}

//2次ベジェ　撃つところ->真ん中の上->当てるところ
Vector3 SpecialBeam::MainPoint(float s) const
{
	Vector3 ctrl = (m_From + m_To) * 0.5f + m_AxisU * kMainArc;
	float u = 1.0f - s;
	return m_From * (u * u) + ctrl * (2.0f * u * s) + m_To * (s * s);
}

//tailとheadの位置と濃さ
void SpecialBeam::MainHeadTail(float t, float& head, float& tail, float& fade) const
{
	head = 1.0f;
	tail = 0.0f;
	fade = 1.0f;
	if (t < kMainGrow)
	{
		float g = std::max(0.0f, t) / kMainGrow;
		head = g * g;	//ちょっとずつ速く伸びて勢いよく当たる
	}
	else if (t >= kMainGrow + kMainHold)
	{
		float f = std::min(1.0f, (t - kMainGrow - kMainHold) / kMainFade);
		tail = f * f * (3.0f - 2.0f * f);	//根元が当てるところの方へ縮んでいく
		fade = 1.0f - f;
	}
}

void SpecialBeam::Fire(const Vector3& from, const Vector3& to, float chargeRatio, bool critical)
{
	m_From = from;
	m_To = to;
	m_Ratio = std::clamp(chargeRatio, 0.0f, 1.0f);
	m_Critical = critical;
	m_Time = 0.0f;
	m_Active = true;
	m_HitSent = false;

	//進む向きと、それに対する「横」「上」
	Vector3 d = to - from;
	float len = d.Length();
	if (len < 1.0f) { len = 1.0f; d = Vector3(0.0f, 0.0f, 1.0f); }
	d.Normalize();
	m_AxisV = Vector3::Up.Cross(d);
	if (m_AxisV.LengthSquared() < 0.0001f) m_AxisV = Vector3(1.0f, 0.0f, 0.0f);
	m_AxisV.Normalize();
	m_AxisU = d.Cross(m_AxisV);
	m_AxisU.Normalize();

	//群れ：1本ずつ、ふくらむ向き,大きさ,遅れ.速さ.太さ.色を変える
	//向きは「真上を0」として ±kSwarmSpreadDeg の範囲を本数で均等に分け、少しだけずらす
	//（ただのランダムだと偏ることがあるので、均等＋ずらし にしている）
	m_Strands.clear();
	const float spread = kSwarmSpreadDeg * RibbonShapes::kPi / 180.0f;
	std::uniform_int_distribution<int> pick(0, kPaletteCount - 1);
	for (int i = 0; i < kSwarmCount; i++)
	{
		float slot = (i + Rand(0.0f, 1.0f)) / (float)kSwarmCount;	//0～1
		float ang = -spread + 2.0f * spread * slot;
		Vector3 off = m_AxisU * std::cos(ang) + m_AxisV * std::sin(ang);

		float ang2 = ang + Rand(-0.8f, 0.8f);	//当てる側は少し違う向きにして、ねじれた弧にする
		Vector3 off2 = m_AxisU * std::cos(ang2) + m_AxisV * std::sin(ang2);

		float r1 = Rand(kSwarmRadiusMin, kSwarmRadiusMax);
		float r2 = r1 * Rand(0.2f, 0.5f);

		Strand s;
		s.c1 = from + d * (len * Rand(0.10f, 0.35f)) + off * r1;
		s.c2 = to - d * (len * Rand(0.10f, 0.30f)) + off2 * r2;
		s.delay = Rand(0.0f, kSwarmDelayMax);
		s.grow = Rand(kSwarmGrowMin, kSwarmGrowMax);
		s.width = Rand(kSwarmWidthMin, kSwarmWidthMax) * (0.6f + 0.4f * m_Ratio);
		s.color = kSwarmPalette[pick(m_Rng)];
		s.arrived = false;
		m_Strands.push_back(s);
	}

	m_BoltTimer = 0.0f;
	RefreshBolts();
}

//雷のエフェクトを作り直す（両端はくっつくように0）
void SpecialBeam::RefreshBolts()
{
	m_BoltOffset.resize(kBolts * (kBoltSegments + 1));
	for (int b = 0; b < kBolts; b++)
	{
		m_BoltWidth[b] = Rand(kBoltWidthMin, kBoltWidthMax);
		for (int i = 0; i <= kBoltSegments; i++)
		{
			float s = i / (float)kBoltSegments;
			float env = std::sin(RibbonShapes::kPi * s);	//真ん中ほど大きく外れる
			m_BoltOffset[b * (kBoltSegments + 1) + i] =
				Vector3(Rand(-1.0f, 1.0f) * env, Rand(-1.0f, 1.0f) * env, 0.0f);
		}
	}
}

void SpecialBeam::Update(float deltaTime)
{
	if (!m_Active) return;
	m_Time += deltaTime;

	//最初の帯の先が敵に届いた瞬間に一回通知
	if (!m_HitSent && m_Time >= kMainGrow)
	{
		m_HitSent = true;
		if (m_OnHit)m_OnHit(m_Ratio, m_Critical, m_To);
	}
	//群れの光が当たるたびに通知
	for (auto& s : m_Strands)
	{
		if (!s.arrived && m_Time >= s.delay + s.grow)
		{
			s.arrived = true;
			if (m_OnSubHit) m_OnSubHit(m_To, s.color);
		}
	}

	//エフェクト
	m_BoltTimer += deltaTime;
	if (m_BoltTimer >= kBoltRefresh)
	{
		m_BoltTimer = 0.0f;
		RefreshBolts();
	}

	if (m_Time >= TotalDuration()) m_Active = false;
}

void SpecialBeam::Draw(Camera* cam)
{
	if (!m_Active) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);

	//群集
	for (const auto& s : m_Strands)
	{
		float t = m_Time - s.delay;
		if (t <= 0.0f) continue;

		float head, tail;
		if (t < s.grow)
		{
			float g = t / s.grow;
			head = g * g;
			tail = std::max(0.0f, head - kSwarmTrail);
		}
		else
		{
			float f = std::min(1.0f, (t - s.grow) / kSwarmTailTime);
			head = 1.0f;
			tail = (1.0f - kSwarmTrail) + kSwarmTrail * f;	//当たった後は尾が吸い込まれていく
		}
		if (head - tail < 0.005f) continue;

		m_Points.clear();
		for (int i = 0; i <= kSwarmSegments; i++)
		{
			float r = i / (float)kSwarmSegments;
			LongBeamRibbon::Point p;
			p.pos = Cubic(m_From, s.c1, s.c2, m_To, tail + (head - tail) * r);
			p.color = RibbonShapes::MakeColor(s.color, r);	//尾は透明、先ほど濃い
			m_Points.push_back(p);
		}
		m_Ribbon.Draw(cam, m_Points, s.width, 1.3f + 0.5f * m_Ratio, -m_Time * 3.0f, s.color * 0.8f);
	}

	//帯
	float mainHead, mainTail, mainFade;
	MainHeadTail(m_Time, mainHead, mainTail, mainFade);
	if (mainHead - mainTail > 0.01f && mainFade > 0.0f)
	{
		//当たった直後だけ太く明るく、そのあと小さく脈打つ
		float pulse = 1.0f;
		if (m_Time >= kMainGrow)
		{
			float a = m_Time - kMainGrow;
			pulse = 1.0f + 0.5f * std::exp(-a * 12.0f) + 0.08f * std::sin(a * 40.0f);
		}
		float width = (kMainWidthMin + (kMainWidthMax - kMainWidthMin) * m_Ratio) * pulse * (0.4f + 0.6f * mainFade);
		float intensity = (0.6f + 0.8f * m_Ratio) * pulse * mainFade;

		m_Points.clear();
		for (int i = 0; i <= kMainSegments; i++)
		{
			float r = i / (float)kMainSegments;
			LongBeamRibbon::Point p;
			p.pos = MainPoint(mainTail + (mainHead - mainTail) * r);
			p.color = Color(1.0f, 0.92f, 0.65f, 0.35f + 0.65f * r);	//芯は白っぽい金
			m_Points.push_back(p);
		}
		m_Ribbon.Draw(cam, m_Points, width, intensity, -m_Time * 2.0f, Vector3(1.0f, 0.55f, 0.12f));
	}

	//エフェクト
	const float boltStart = kMainGrow * 0.6f;
	const float boltEnd = kMainGrow + kMainHold + kMainFade * 0.6f;
	if (m_Time >= boltStart && m_Time <= boltEnd && mainHead - mainTail > 0.01f)
	{
		float boltFade = 1.0f;
		if (m_Time > kMainGrow + kMainHold)
			boltFade = std::clamp(1.0f - (m_Time - kMainGrow - kMainHold) / (kMainFade * 0.6f), 0.0f, 1.0f);

		for (int b = 0; b < kBolts; b++)
		{
			m_Points.clear();
			for (int i = 0; i <= kBoltSegments; i++)
			{
				float r = i / (float)kBoltSegments;
				const Vector3& o = m_BoltOffset[b * (kBoltSegments + 1) + i];
				LongBeamRibbon::Point p;
				p.pos = MainPoint(mainTail + (mainHead - mainTail) * r) + (m_AxisU * o.x + m_AxisV * o.y) * kBoltJitter;
				p.color = Color(0.85f, 0.95f, 1.0f, boltFade);
				m_Points.push_back(p);
			}
			float flicker = Rand(1.2f, 2.2f);	//明るさもチカチカ変える
			m_Ribbon.Draw(cam, m_Points, m_BoltWidth[b], flicker * (0.6f + 0.6f * m_Ratio), m_Time * 7.0f,
				Vector3(0.4f, 0.6f, 1.0f));
		}
	}

	Renderer::SetBlendState(BS_NONE);
}
