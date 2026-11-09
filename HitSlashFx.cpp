#include "HitSlashFx.h"
#include "RibbonShapes.h"
#include "GameEvent.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void HitSlashFx::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(32);
	m_Slashes.reserve(kMaxSlashes);
	m_Shards.reserve(kMaxShards);
	m_Rng.seed(std::random_device{}());

	//叩いたを聞いて判定ごとに斬撃と破片を出す
	m_JudgeSub = GameEvent::OnJudge().Subscribe([this](const GameEvent::Judge& e)
		{
			Vector3 p = e.pos + Vector3(0.0f, kLift, 0.0f);
			float tilt = Rand(0.35f, 0.7f) * (Rand(0.0f, 1.0f) < 0.5f ? 1.0f : -1.0f);	//20~40度、左右どちらか

			switch (e.rank)
			{
			case 0:	//WEAK:金の十字
			{
				const Vector3 gold(1.0f, 0.82f, 0.35f);
				SpawnSlash(p, gold, 0.6f, 260.0f, 34.0f, kSlashLife * 1.3f);
				SpawnSlash(p, gold, -0.6f, 260.0f, 34.0f, kSlashLife * 1.3f);
				SpawnShards(p, gold, 16, 350.0f, 800.0f);
				SpawnShards(p, e.color, 6, 250.0f, 600.0f);
				break;
			}
			case 1:	//HIT:レーンの色の斬撃1本
				SpawnSlash(p, e.color, tilt, 210.0f, 26.0f, kSlashLife);
				SpawnShards(p, e.color, 9, 280.0f, 650.0f);
				break;
			default:	//GRAZE:細く短く暗く
				SpawnSlash(p, e.color * 0.7f, tilt, 140.0f, 14.0f, kSlashLife * 0.8f);
				SpawnShards(p, e.color * 0.7f, 4, 200.0f, 450.0f);
				break;
			}
		});
}

float HitSlashFx::Rand(float a, float b)
{
	std::uniform_real_distribution<float> d(a, b);
	return d(m_Rng);
}

void HitSlashFx::Update()
{
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted) { m_LastClock = now; m_ClockStarted = true; }
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	dt = std::min(dt, 0.1f);	//止まった時に一気に進まないように

	for (auto& s : m_Slashes) s.age += dt;
	m_Slashes.erase(std::remove_if(m_Slashes.begin(), m_Slashes.end(),
		[](const Slash& s) { return s.age >= s.life; }), m_Slashes.end());

	for (auto& s : m_Shards)
	{
		s.age += dt;
		s.vel.y -= kGravity * dt;
		s.vel *= std::max(0.0f, 1.0f - kDrag * dt);
		s.pos += s.vel * dt;
	}
	m_Shards.erase(std::remove_if(m_Shards.begin(), m_Shards.end(),
		[](const Shard& s) { return s.age >= s.life; }), m_Shards.end());
}

void HitSlashFx::Draw(Camera* cam)
{
	if (m_Slashes.empty() && m_Shards.empty()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);
	const Vector3 camPos = cam->GetPosition();

	//斬撃：反った刃が端から端まで走り、後ろから消えていく
	for (const auto& s : m_Slashes)
	{
		float k = s.age / s.life;
		float head = std::min(1.0f, s.age / kSweepTime);	//刃の先：すばやく端まで走る
		head = 1.0f - (1.0f - head) * (1.0f - head);
		float tail = std::clamp((s.age - kSweepTime * 0.5f) / (s.life - kSweepTime * 0.5f), 0.0f, 1.0f);	//後ろから消す
		tail = tail * tail;
		if (head - tail < 0.01f) continue;
		float fade = 1.0f - k * k;

		//カメラの方を向いた面の上で、刃の向き(ax)と反りの向き(ay)を決める
		Vector3 right, up;
		RibbonShapes::CameraBasis(s.pos, camPos, right, up);
		float ca = std::cos(s.angle), sa = std::sin(s.angle);
		Vector3 ax = right * ca + up * sa;
		Vector3 ay = up * ca - right * sa;

		m_Pts.clear();
		constexpr int kSeg = 16;
		for (int i = 0; i <= kSeg; i++)
		{
			float local = i / (float)kSeg;
			float t = tail + (head - tail) * local;	//刃の上の位置 0〜1
			float u = t * 2.0f - 1.0f;				//-1〜1
			LongBeamRibbon::Point pt;
			pt.pos = s.pos + ax * (u * s.length * 0.5f) + ay * ((1.0f - u * u) * kBow * s.length);
			float edge = std::sin(local * RibbonShapes::kPi);	//両端を薄くして、刃の形に見せる
			Vector3 c = s.color + (Vector3(1.0f, 1.0f, 1.0f) - s.color) * 0.0f;	//少し白を混ぜる
			pt.color = RibbonShapes::MakeColor(c, edge * fade);
			m_Pts.push_back(pt);
		}
		m_Ribbon.Draw(cam, m_Pts, s.width, 1.6f, s.age, s.color);	//色の光

		for (auto& pt : m_Pts) pt.color = Color(1.0f, 1.0f, 1.0f, pt.color.w);
		m_Ribbon.Draw(cam, m_Pts, s.width * 0.35f, 2.0f, s.age, Vector3(1.0f, 1.0f, 1.0f));	//白い芯
	}

	//破片：尾を引く
	for (const auto& s : m_Shards)
	{
		float fade = 1.0f - std::clamp(s.age / s.life, 0.0f, 1.0f);
		Vector3 tail = s.pos - s.vel * 0.03f;

		m_Pts.clear();
		LongBeamRibbon::Point p;
		p.pos = tail;                  p.color = RibbonShapes::MakeColor(s.color, 0.0f);         m_Pts.push_back(p);
		p.pos = (tail + s.pos) * 0.5f; p.color = RibbonShapes::MakeColor(s.color, 0.7f * fade);  m_Pts.push_back(p);
		p.pos = s.pos;                 p.color = RibbonShapes::MakeColor(s.color, fade);         m_Pts.push_back(p);
		m_Ribbon.Draw(cam, m_Pts, s.width, 1.5f, s.age, s.color);
	}
	Renderer::SetBlendState(BS_NONE);
}

void HitSlashFx::Uninit()
{
	GameEvent::OnJudge().Unsubscribe(m_JudgeSub);	//聞くのをやめる
	m_Slashes.clear();
	m_Shards.clear();
}

void HitSlashFx::SpawnSlash(const Vector3& pos, const Vector3& color, float angle, float length, float width, float life)
{
	if ((int)m_Slashes.size() >= kMaxSlashes) m_Slashes.erase(m_Slashes.begin());	//多すぎたら古いものから消す
	m_Slashes.push_back({ pos,color,angle,length,width,0.0f,life });
}

void HitSlashFx::SpawnShards(const Vector3& pos, const Vector3& color, int count, float speedMin, float speedMax)
{
	for (int i = 0; i < count; i++)
	{
		if ((int)m_Shards.size() >= kMaxShards) m_Shards.erase(m_Shards.begin());
		//上と置くへ飛ばす。レーンには落とさない
		Vector3 dir(Rand(-1.0f, 1.0f), Rand(0.6f, 1.4f), Rand(0.2f, 1.0f));
		dir.Normalize();
		Shard s;
		s.pos = pos;
		s.vel = dir * Rand(speedMin, speedMax);
		s.color = color;
		s.width = Rand(6.0f, 11.0f);
		s.age = 0.0f;
		s.life = Rand(0.3f, 0.5f);
		m_Shards.push_back(s);
	}
}
