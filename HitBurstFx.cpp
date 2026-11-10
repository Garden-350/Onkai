#include "HitBurstFx.h"
#include "RibbonShapes.h"
#include "GameEvent.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void HitBurstFx::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(40);
	m_Bursts.reserve(kMaxBursts);
	m_Lasers.reserve(kMaxLasers);
	m_Rng.seed(std::random_device{}());

	//叩いた：その場で破片になってはじける（ロングの途中の判定は今まで通りなので出さない）
	m_JudgeSub = GameEvent::OnJudge().Subscribe([this](const GameEvent::Judge& e)
		{
			if (e.isLong) return;
			SpawnBurst(e.pos + Vector3(0.0f, kLift, 0.0f), e.rank);
		});

	//拍の頭：待っている破片を、1つ1つレーザーにして撃つ
	m_BeatSub = GameEvent::OnBeat().Subscribe([this](const GameEvent::Beat& e)
		{
			if (e.index % kFireEveryBeats == 0) FireReady();
		});
}

float HitBurstFx::Rand(float a, float b)
{
	std::uniform_real_distribution<float> d(a, b);
	return d(m_Rng);
}

void HitBurstFx::SpawnBurst(const Vector3& pos, int rank)
{
	if ((int)m_Bursts.size() >= kMaxBursts) m_Bursts.erase(m_Bursts.begin());	//多すぎたら古いものから消す

	Burst b;
	b.rank = rank;
	b.color = RankColor(rank);
	int count = (rank == 0) ? 14 : (rank == 1) ? 10 : 6;	//WEAKほど多い
	float speed = (rank == 0) ? 520.0f : (rank == 1) ? 420.0f : 300.0f;
	for (int i = 0; i < count; i++)
	{
		//上の方へ、ばらばらに飛び散る（レーンの上に落ちてこないように）
		Vector3 dir(Rand(-1.0f, 1.0f), Rand(0.2f, 1.2f), Rand(-0.4f, 0.6f));
		dir.Normalize();
		Piece p;
		p.pos = pos;
		p.vel = dir * speed * Rand(0.6f, 1.2f);
		p.width = Rand(6.0f, 11.0f) * (rank == 0 ? 1.3f : 1.0f);
		p.spin = Rand(0.0f, 6.28f);
		b.piece.push_back(p);
	}
	m_Bursts.push_back(b);
}

void HitBurstFx::FireReady()
{
	for (auto& b : m_Bursts)
		if (b.age >= kMinHover) FireBurst(b);	//はじけた直後のものは次の拍に回す
}

void HitBurstFx::FireBurst(Burst& b)
{
	for (size_t i = 0; i < b.piece.size(); i++)
	{
		bool notify = (i == 0);	//当たったを知らせるのは、かたまりの最初の1本だけ
		float delay = notify ? 0.0f : Rand(0.0f, kFireSpread);
		LaunchLaser(b.piece[i].pos, b.color, b.rank, notify, delay);
	}
	b.piece.clear();	//破片はレーザーになったので消す（Updateでかたまりごと片付く）
}

void HitBurstFx::LaunchLaser(const Vector3& from, const Vector3& color, int rank, bool notify, float delay)
{
	if ((int)m_Lasers.size() >= kMaxLasers) m_Lasers.erase(m_Lasers.begin());

	Laser l;
	l.from = from;
	l.to = m_Target + Vector3(Rand(-90.0f, 90.0f), Rand(-60.0f, 90.0f), Rand(-40.0f, 40.0f));	//敵の体のあちこち

	//どっちから撃つか：右のレーンの破片は敵の右側から、左は左側から
	//角度は真横の少し下(-15度)〜ほぼ真上(85度)。真下（レーンのある方）からは撃たない
	float side = (from.x >= m_Target.x) ? 1.0f : -1.0f;
	float rad = DirectX::XMConvertToRadians(Rand(-15.0f, 85.0f));
	Vector3 dir(side * std::cos(rad), std::sin(rad), Rand(-0.3f, 0.2f));
	dir.Normalize();
	l.launch = l.to + dir * Rand(kLaunchDistMin, kLaunchDistMax);	//敵のまわりの発射位置

	l.color = color;
	l.rank = rank;
	l.width = (rank == 0) ? 8.0f : (rank == 1) ? 6.0f : 4.5f;
	l.flareSize = (rank == 0) ? 46.0f : (rank == 1) ? 34.0f : 24.0f;	//WEAKほど大きく光る
	l.flareRot = Rand(0.0f, 1.57f);
	l.age = -delay;
	l.notify = notify;
	m_Lasers.push_back(l);
}

//光の玉（レンズフレア風）：ぼんやりした光 ＋ 十字の光の筋 ＋ 白い芯
void HitBurstFx::DrawFlare(Camera* cam, const Vector3& pos, const Vector3& color, float size, float rot, float alpha)
{
	if (alpha <= 0.0f) return;
	Vector3 right, up;
	RibbonShapes::CameraBasis(pos, cam->GetPosition(), right, up);	//カメラの方を向いた面の右と上
	const Vector3 white(1.0f, 1.0f, 1.0f);
	const Vector3 light = color + (white - color) * 0.5f;	//色に白を混ぜた明るい色

	//真ん中が一番濃く、両端に向かって消える帯を、angleの向きに1本描く
	auto streak = [&](float angle, float len, float width, float intensity, const Vector3& c)
		{
			Vector3 d = right * std::cos(angle) + up * std::sin(angle);
			m_Pts.clear();
			LongBeamRibbon::Point p;
			p.pos = pos - d * len; p.color = RibbonShapes::MakeColor(c, 0.0f);  m_Pts.push_back(p);
			p.pos = pos;           p.color = RibbonShapes::MakeColor(c, alpha); m_Pts.push_back(p);
			p.pos = pos + d * len; p.color = RibbonShapes::MakeColor(c, 0.0f);  m_Pts.push_back(p);
			m_Ribbon.Draw(cam, m_Pts, width, intensity, 0.0f, c);
		};

	streak(rot, size * 0.9f, size * 1.8f, 0.35f, color);				//ぼんやりした光
	streak(rot + 1.5708f, size * 0.9f, size * 1.8f, 0.35f, color);
	streak(rot, size * 2.8f, size * 0.12f, 0.9f, color);				//長い光の筋（横）
	streak(rot + 1.5708f, size * 2.0f, size * 0.12f, 0.9f, color);	//長い光の筋（縦）
	streak(rot, size * 0.2f, size * 0.4f, 1.0f, white);				//小さな白い芯
}

void HitBurstFx::Update()
{
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted) { m_LastClock = now; m_ClockStarted = true; }
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	dt = std::min(dt, 0.1f);

	//破片：はじける → 漂って、拍の頭を待つ
	for (auto& b : m_Bursts)
	{
		b.age += dt;
		for (auto& p : b.piece)
		{
			p.vel *= std::max(0.0f, 1.0f - kDrag * dt);	//すぐ減速して、その場に漂う
			p.vel.y += 30.0f * dt;						//ほんの少し浮く
			Vector3 sway(std::sin(b.age * 7.0f + p.spin), std::cos(b.age * 6.0f + p.spin), 0.0f);
			p.pos += p.vel * dt + sway * 6.0f * dt;		//小さく揺れる
		}
		if (!b.piece.empty() && b.age >= kMaxWait) FireBurst(b);	//拍が来なくても撃つ
	}
	m_Bursts.erase(std::remove_if(m_Bursts.begin(), m_Bursts.end(),
		[](const Burst& b) { return b.piece.empty(); }), m_Bursts.end());

	//レーザー：届いたら「当たった」を知らせる
	std::vector<GameEvent::AttackHit> arrived;	//ループの外でまとめて知らせる
	for (auto& l : m_Lasers)
	{
		l.age += dt;
		if (!l.arrived && l.age >= kDashTime + kLaserTime)
		{
			l.arrived = true;
			if (l.notify) arrived.push_back({ l.to, l.color, l.rank });
		}
	}
	m_Lasers.erase(std::remove_if(m_Lasers.begin(), m_Lasers.end(),
		[](const Laser& l)
		{
			float end = kDashTime + std::max(kLaserTime * (1.0f + kLaserTail), kLaserTime + kHitFlashTime);
			return l.age >= end;	//尾まで届いて、光の玉も消えたら消す
		}), m_Lasers.end());

	for (const auto& h : arrived) GameEvent::OnAttackHit().Publish(h);
}

void HitBurstFx::Draw(Camera* cam)
{
	if (m_Bursts.empty() && m_Lasers.empty()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);

	//破片：動いている向きに短い尾を引く。漂っている間は小さく瞬く
	for (const auto& b : m_Bursts)
	{
		float twinkle = 1.0f + 0.3f * std::sin(b.age * 30.0f);
		for (const auto& p : b.piece)
		{
			float sp = p.vel.Length();
			Vector3 d = (sp > 1.0f) ? p.vel / sp : Vector3(std::cos(p.spin), std::sin(p.spin), 0.0f);
			float len = std::max(12.0f, sp * 0.03f);

			m_Pts.clear();
			LongBeamRibbon::Point pt;
			pt.pos = p.pos - d * len;        pt.color = RibbonShapes::MakeColor(b.color, 0.0f); m_Pts.push_back(pt);
			pt.pos = p.pos - d * len * 0.5f; pt.color = RibbonShapes::MakeColor(b.color, 0.7f); m_Pts.push_back(pt);
			pt.pos = p.pos;                  pt.color = RibbonShapes::MakeColor(b.color, 1.0f); m_Pts.push_back(pt);
			m_Ribbon.Draw(cam, m_Pts, p.width, 1.5f * twinkle, b.age, b.color);
		}
	}

	//レーザー:発射位置へ飛ぶ,まっすぐ撃つ
	for (const auto& l : m_Lasers)
	{
		if (l.age < 0.0f) continue;	//まだ撃つ前（ずらしの待ち）

		//破片が発射位置へすっと飛ぶ（小さな光の点と短い尾）
		if (l.age < kDashTime)
		{
			float u = l.age / kDashTime;
			u = 1.0f - (1.0f - u) * (1.0f - u);	//最初は速く、着く前にゆっくり
			Vector3 p = Vector3::Lerp(l.from, l.launch, u);
			Vector3 d = l.launch - l.from;
			d.Normalize();

			m_Pts.clear();
			LongBeamRibbon::Point pt;
			pt.pos = p - d * 40.0f; pt.color = RibbonShapes::MakeColor(l.color, 0.0f); m_Pts.push_back(pt);
			pt.pos = p;             pt.color = RibbonShapes::MakeColor(l.color, 0.9f); m_Pts.push_back(pt);
			m_Ribbon.Draw(cam, m_Pts, l.width * 1.2f, 0.8f, l.age, l.color);
			DrawFlare(cam, p, l.color, l.flareSize * 0.5f, l.flareRot, 0.8f);
			continue;
		}

		//発射位置から敵へ、まっすぐな光の線
		float k = (l.age - kDashTime) / kLaserTime;
		float head = Ease(std::min(1.0f, k));
		float tail = Ease(std::clamp(k - kLaserTail, 0.0f, 1.0f));
		Vector3 headPos = Vector3::Lerp(l.launch, l.to, head);
		Vector3 tailPos = Vector3::Lerp(l.launch, l.to, tail);

		if (head - tail > 0.005f)
		{
			m_Pts.clear();	//まっすぐなので点は3つで足りる
			LongBeamRibbon::Point pt;
			pt.pos = tailPos;                      pt.color = RibbonShapes::MakeColor(l.color, 0.0f); m_Pts.push_back(pt);
			pt.pos = (tailPos + headPos) * 0.5f;   pt.color = RibbonShapes::MakeColor(l.color, 0.4f); m_Pts.push_back(pt);
			pt.pos = headPos;                      pt.color = RibbonShapes::MakeColor(l.color, 1.0f); m_Pts.push_back(pt);

			m_Ribbon.Draw(cam, m_Pts, l.width * 3.0f, 0.35f, l.age, l.color);	//まわりのぼんやりした光（色）
			m_Ribbon.Draw(cam, m_Pts, l.width, 0.9f, l.age, l.color);			//色の線
			for (auto& p : m_Pts) p.color = Color(1.0f, 1.0f, 1.0f, p.color.w * 0.7f);
			m_Ribbon.Draw(cam, m_Pts, l.width * 0.25f, 0.9f, l.age, l.color);	//細い白い芯
		}

		//光の玉：飛んでいる間は先に付いて瞬き、届いた瞬間に大きく広がって消える
		if (k < 1.0f)
		{
			float twinkle = 0.85f + 0.15f * std::sin(l.age * 60.0f + l.flareRot * 10.0f);
			DrawFlare(cam, headPos, l.color, l.flareSize * twinkle, l.flareRot + l.age * 2.0f, 1.0f);
		}
		else
		{
			float f = (l.age - kDashTime - kLaserTime) / kHitFlashTime;	//届いてからの割合 0〜1
			if (f < 1.0f) DrawFlare(cam, l.to, l.color, l.flareSize * (1.0f + 0.8f * f), l.flareRot, 1.0f - f);
		}
	}

}

void HitBurstFx::Uninit()
{
	GameEvent::OnJudge().Unsubscribe(m_JudgeSub);
	GameEvent::OnBeat().Unsubscribe(m_BeatSub);
	m_Bursts.clear();
	m_Lasers.clear();
}
