#include "ImpactFx.h"
#include "RibbonShapes.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void ImpactFx::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(64);
	m_Rings.reserve(kMaxRings);
	m_Sparks.reserve(kMaxSparks);
	m_Rng.seed(std::random_device{}());
}

void ImpactFx::Clear()
{
	m_Rings.clear();
	m_Sparks.clear();
}

float ImpactFx::Rand(float a, float b)
{
	std::uniform_real_distribution<float> d(a, b);
	return d(m_Rng);
}

// 出す（種類ごと）
void ImpactFx::SpawnSmallHit(const Vector3& pos, const Vector3& color)
{
	Vector3 c = color * kSmallHitBright;
	AddRing(pos, c, 0, 8.0f, 55.0f, 0.22f, 7.0f);								// 小さい輪
	AddSparks(pos, c, 5, 200.0f, 420.0f, 0.25f, 5.0f, Vector3::Zero, 0.0f);	// 少しの火花
}

void ImpactFx::SpawnWeakHit(const Vector3& pos, const Vector3& color)
{
	const Vector3 gold(1.0f, 0.85f, 0.4f);
	AddRing(pos, gold, 0, 10.0f, 110.0f, 0.30f, 10.0f);							// 金の輪
	AddRing(pos, color, 6, 20.0f, 75.0f, 0.28f, 7.0f, Rand(0.0f, 1.0f));		// 六角形
	AddSparks(pos, gold, 12, 300.0f, 700.0f, 0.35f, 6.0f, Vector3::Up, 0.2f);	// 上向きに多めの火花
}

void ImpactFx::SpawnCritical(const Vector3& pos, const Vector3& color, float power)
{
	power = std::clamp(power, 0.0f, 1.0f);
	const Vector3 gold(1.0f, 0.85f, 0.4f);
	const Vector3 white(1.0f, 1.0f, 1.0f);

	AddRing(pos, white, 0, 10.0f, 80.0f + 60.0f * power, 0.16f, 30.0f);					// 白い閃光（すぐ消える）
	AddRing(pos, gold, 0, 20.0f, 200.0f + 220.0f * power, 0.45f, 14.0f);					// 衝撃の輪
	AddRing(pos, color, 6, 30.0f, 130.0f + 120.0f * power, 0.35f, 9.0f, Rand(0.0f, 1.0f));	// 六角形
	AddSparks(pos, gold, 18 + (int)(30.0f * power), 450.0f, 1100.0f, 0.5f, 8.0f, Vector3::Up, 0.25f);
	AddSparks(pos, color, 10 + (int)(10.0f * power), 300.0f, 750.0f, 0.4f, 6.0f, Vector3::Zero, 0.0f);
}

void ImpactFx::SpawnBlocked(const Vector3& pos, const Vector3& fromDir)
{
	const Vector3 shield(0.55f, 0.75f, 1.0f);	// 青白い盾
	AddRing(pos, shield, 6, 70.0f, 95.0f, 0.45f, 12.0f);			// 盾（六角形が出て、少し広がって消える）
	AddRing(pos, shield, 6, 40.0f, 60.0f, 0.30f, 6.0f, 0.5236f);	// 内側の六角形（30度回す）
	AddSparks(pos, shield, 16, 300.0f, 700.0f, 0.35f, 5.0f, -fromDir, 0.75f);	// 来た方へはね返る
}

// ============================================================
// 中身を足す
// ============================================================
void ImpactFx::AddRing(const Vector3& pos, const Vector3& color, int sides, float r0, float r1,
	float life, float width, float rot)
{
	if ((int)m_Rings.size() >= kMaxRings) m_Rings.erase(m_Rings.begin());	// 一番古いものを消す

	Ring r;
	r.pos = pos;
	r.color = color;
	r.sides = sides;
	r.r0 = r0;
	r.r1 = r1;
	r.width = width;
	r.rot = rot;
	r.age = 0.0f;
	r.life = life;
	m_Rings.push_back(r);
}

void ImpactFx::AddSparks(const Vector3& pos, const Vector3& color, int count, float speedMin, float speedMax,
	float life, float width, const Vector3& bias, float biasAmount)
{
	Vector3 b = bias;
	if (b.LengthSquared() > 0.0001f) b.Normalize();
	biasAmount = std::clamp(biasAmount, 0.0f, 1.0f);

	for (int i = 0; i < count; i++)
	{
		if ((int)m_Sparks.size() >= kMaxSparks) m_Sparks.erase(m_Sparks.begin());	// 一番古いものを消す

		// ばらばらの向きを作って、bias の方へかたよらせる
		Vector3 dir(Rand(-1.0f, 1.0f), Rand(-1.0f, 1.0f), Rand(-1.0f, 1.0f));
		if (dir.LengthSquared() < 0.0001f) dir = Vector3::Up;
		dir.Normalize();
		dir = dir * (1.0f - biasAmount) + b * biasAmount;
		if (dir.LengthSquared() < 0.0001f) dir = Vector3::Up;
		dir.Normalize();

		Spark s;
		s.pos = pos;
		s.vel = dir * Rand(speedMin, speedMax);
		s.color = color;
		s.width = width * Rand(0.7f, 1.2f);
		s.age = 0.0f;
		s.life = life * Rand(0.7f, 1.2f);
		m_Sparks.push_back(s);
	}
}

// ============================================================
// 更新：時間を進めて、寿命が来たものを消す
// ============================================================
void ImpactFx::Update(float deltaTime)
{
	for (auto& r : m_Rings) r.age += deltaTime;
	m_Rings.erase(std::remove_if(m_Rings.begin(), m_Rings.end(),
		[](const Ring& r) { return r.age >= r.life; }), m_Rings.end());

	for (auto& s : m_Sparks)
	{
		s.age += deltaTime;
		s.vel.y -= kGravity * deltaTime;						// 落ちる
		s.vel *= std::max(0.0f, 1.0f - kDrag * deltaTime);	// だんだん遅くなる
		s.pos += s.vel * deltaTime;
	}
	m_Sparks.erase(std::remove_if(m_Sparks.begin(), m_Sparks.end(),
		[](const Spark& s) { return s.age >= s.life; }), m_Sparks.end());
}

// ============================================================
// 描画
// ============================================================
void ImpactFx::Draw(Camera* cam)
{
	if (m_Rings.empty() && m_Sparks.empty()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);
	const Vector3 camPos = cam->GetPosition();

	// ── 輪・六角形：最初に速く広がって、だんだんゆっくり。広がりながら薄くなる ──
	for (const auto& r : m_Rings)
	{
		float k = std::clamp(r.age / r.life, 0.0f, 1.0f);
		float e = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);
		float radius = r.r0 + (r.r1 - r.r0) * e;
		float fade = 1.0f - k;

		Vector3 right, up;
		RibbonShapes::CameraBasis(r.pos, camPos, right, up);
		m_Pts.clear();
		Color c = RibbonShapes::MakeColor(r.color, fade);
		if (r.sides >= 3) RibbonShapes::Polygon(m_Pts, r.pos, right, up, radius, r.sides, r.rot, c);
		else              RibbonShapes::Arc(m_Pts, r.pos, right, up, radius, 0.0f, 2.0f * RibbonShapes::kPi, 40, c);

		m_Ribbon.Draw(cam, m_Pts, r.width * (1.0f - 0.6f * k), 1.3f, r.age, r.color * 0.8f);
	}

	// ── 火花：飛んでいる向きに尾を引く短い線 ──
	for (const auto& s : m_Sparks)
	{
		float fade = 1.0f - std::clamp(s.age / s.life, 0.0f, 1.0f);
		Vector3 tail = s.pos - s.vel * kStreak;

		m_Pts.clear();
		LongBeamRibbon::Point p;
		p.pos = tail;                    p.color = RibbonShapes::MakeColor(s.color, 0.0f);         m_Pts.push_back(p);
		p.pos = (tail + s.pos) * 0.5f;   p.color = RibbonShapes::MakeColor(s.color, 0.6f * fade);  m_Pts.push_back(p);
		p.pos = s.pos;                   p.color = RibbonShapes::MakeColor(s.color, fade);         m_Pts.push_back(p);

		m_Ribbon.Draw(cam, m_Pts, s.width, 1.5f, s.age, s.color);
	}

	Renderer::SetBlendState(BS_NONE);
}
