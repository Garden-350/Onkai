#include "WeakPoint.h"
#include "RibbonShapes.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void WeakPoint::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(64);
	for (int i = 0; i < 6; i++) m_LaneColor[i] = Vector3(1.0f, 1.0f, 1.0f);
	m_Rng.seed(std::random_device{}());
	Reset();
}

void WeakPoint::SetLaneColor(int lane, const Vector3& color)
{
	if (lane >= 0 && lane < 6) m_LaneColor[lane] = color;
}

void WeakPoint::Reset()
{
	m_Mode = Mode::Normal;
	m_WeakLane = -1;
	m_PhaseStart = 0.0f;
	m_PhaseEnd = 0.0f;
	m_Aim = 2;
	m_Locked = false;
	m_LockFlash = 0.0f;
	m_SwitchFlash = 0.0f;
	m_HasCenter = false;
}

// 6つの部位の位置：左から S D F J K L。真ん中ほど少し高い（山なり）
Vector3 WeakPoint::GetPoint(int lane) const
{
	lane = std::clamp(lane, 0, 5);
	float x = (lane - 2.5f) / 2.5f;	// -1（左端）～ +1（右端）
	return m_Center + Vector3((lane - 2.5f) * kPointSpacingX,
		kPointBaseY + kPointArcY * (1.0f - x * x),
		kPointFrontZ);
}

void WeakPoint::PickNewLane()
{
	std::uniform_int_distribution<int> d(0, 5);
	int lane = d(m_Rng);
	if (m_WeakLane >= 0)
	{
		while (lane == m_WeakLane) lane = d(m_Rng);	// 前と同じ所は選ばない
	}
	m_WeakLane = lane;
	m_SwitchFlash = 1.0f;
}

void WeakPoint::Update(float deltaTime, float musicTime, float bpm, int beatsPerBar, const Vector3& enemyCenter)
{
	m_Time += deltaTime;
	m_Now = musicTime;
	m_Center = enemyCenter;
	m_HasCenter = true;
	m_LockFlash = std::max(0.0f, m_LockFlash - deltaTime * 3.0f);
	m_SwitchFlash = std::max(0.0f, m_SwitchFlash - deltaTime * 3.5f);

	float beat = (bpm > 0.0f) ? 60.0f / bpm : 0.5f;
	m_Interval = beat * (float)std::max(1, beatsPerBar) * (float)kBarsPerSwitch;

	if (m_Mode != Mode::Normal) return;	// 必殺技の間は弱点を動かさない
	if (musicTime < 0.0f) return;		// カウントイン中は弱点なし

	if (m_WeakLane < 0)
	{
		// 曲が始まった：最初の弱点を選ぶ
		PickNewLane();
		m_PhaseStart = musicTime;
		m_PhaseEnd = musicTime + m_Interval;
	}
	else if (musicTime >= m_PhaseEnd)
	{
		// 時間が来た：次の部位へ移る
		PickNewLane();
		m_PhaseStart = m_PhaseEnd;
		m_PhaseEnd = m_PhaseStart + m_Interval;
		if (m_PhaseEnd <= musicTime)	// ポーズ明けなどで大きく遅れていたら、今から数え直す
		{
			m_PhaseStart = musicTime;
			m_PhaseEnd = musicTime + m_Interval;
		}
	}
}

// ============================================================
// 必殺技
// ============================================================
void WeakPoint::BeginSpecial(float musicTime, float fireTime, int startAim)
{
	m_Mode = Mode::Aiming;
	PickNewLane();				// 必殺技用に選び直して、撃つまで固定
	m_PhaseStart = musicTime;
	m_PhaseEnd = fireTime;		// 残り時間の輪は「撃つまで」を表す
	m_Locked = false;
	m_Aim = std::clamp(startAim, 0, 5);
}

void WeakPoint::SetAim(int lane)
{
	if (m_Mode != Mode::Aiming || m_Locked) return;
	if (lane < 0 || lane >= 6) return;
	m_Aim = lane;
}

bool WeakPoint::Lock()
{
	if (m_Mode != Mode::Aiming || m_Locked) return false;
	m_Locked = true;
	m_LockFlash = 1.0f;
	return true;
}

void WeakPoint::OnFired()
{
	m_Mode = Mode::Fired;
}

void WeakPoint::EndSpecial(float musicTime)
{
	m_Mode = Mode::Normal;
	m_Locked = false;
	PickNewLane();
	m_PhaseStart = musicTime;
	m_PhaseEnd = musicTime + m_Interval;
}

// ============================================================
// 描画
// ============================================================
void WeakPoint::Flush(Camera* cam, float width, float intensity, const Vector3& glow)
{
	if (m_Pts.size() >= 2) m_Ribbon.Draw(cam, m_Pts, width, intensity, m_Time, glow);
	m_Pts.clear();
}

void WeakPoint::Draw(Camera* cam)
{
	if (!m_HasCenter) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);
	const Vector3 camPos = cam->GetPosition();
	const float kTwoPi = 2.0f * RibbonShapes::kPi;
	Vector3 right, up;
	m_Pts.clear();

	// ── 6つの部位の小さい印（どのキーがどこか分かるように、レーンの色で薄く）──
	if (kMarkAlpha > 0.0f)
	{
		for (int lane = 0; lane < 6; lane++)
		{
			if (lane == m_WeakLane) continue;
			Vector3 p = GetPoint(lane);
			RibbonShapes::CameraBasis(p, camPos, right, up);
			RibbonShapes::Arc(m_Pts, p, right, up, kMarkRadius, 0.0f, kTwoPi, 20,
				RibbonShapes::MakeColor(m_LaneColor[lane], kMarkAlpha));
			Flush(cam, 5.0f, 1.0f, m_LaneColor[lane]);
		}
	}

	// ── 弱点 ──
	if (m_WeakLane >= 0)
	{
		Vector3 p = GetPoint(m_WeakLane);
		Vector3 col = m_LaneColor[m_WeakLane];
		RibbonShapes::CameraBasis(p, camPos, right, up);

		float len = m_PhaseEnd - m_PhaseStart;
		float remain = (len > 0.0001f) ? std::clamp((m_PhaseEnd - m_Now) / len, 0.0f, 1.0f) : 0.0f;
		float blink = (remain < 0.25f) ? 0.55f + 0.45f * std::sin(m_Time * 30.0f) : 1.0f;	// 残りわずかで点滅
		float pulse = 1.0f + 0.12f * std::sin(m_Time * 6.0f);

		// 六角形（ゆっくり回る）
		RibbonShapes::Polygon(m_Pts, p, right, up, kWeakRadius * pulse, 6, m_Time * 0.8f,
			RibbonShapes::MakeColor(col, blink));
		Flush(cam, 9.0f, 1.6f, col);

		// 真ん中の点
		RibbonShapes::Arc(m_Pts, p, right, up, 8.0f, 0.0f, kTwoPi, 16, Color(1.0f, 1.0f, 1.0f, blink));
		Flush(cam, 10.0f, 2.0f, col);

		// 残り時間の輪：真上から時計回りに、残っている分だけ
		if (remain > 0.001f)
		{
			int segs = std::max(4, (int)(48.0f * remain));
			Vector3 ringCol = col * 0.3f + Vector3(0.7f, 0.7f, 0.7f);	// 白寄りのレーン色
			RibbonShapes::Arc(m_Pts, p, right, up, kTimerRadius, 0.0f, kTwoPi * remain, segs,
				RibbonShapes::MakeColor(ringCol, blink));
			Flush(cam, 6.0f, 1.2f, col);
		}

		// 移った瞬間：外へ広がって消える輪
		if (m_SwitchFlash > 0.0f)
		{
			RibbonShapes::Arc(m_Pts, p, right, up, kWeakRadius + (1.0f - m_SwitchFlash) * 70.0f, 0.0f, kTwoPi, 32,
				RibbonShapes::MakeColor(col, m_SwitchFlash));
			Flush(cam, 10.0f, 1.5f, col);
		}
	}

	// ── 狙いの枠（必殺技のチャージ中だけ）──
	if (m_Mode == Mode::Aiming)
	{
		Vector3 p = GetPoint(m_Aim);
		RibbonShapes::CameraBasis(p, camPos, right, up);

		bool match = kShowMatchHint && (m_Aim == m_WeakLane);
		Vector3 col = m_Locked ? Vector3(1.0f, 0.95f, 0.7f)
			: (match ? Vector3(1.0f, 0.8f, 0.25f) : Vector3(0.75f, 0.85f, 1.0f));
		float radius = m_Locked ? kLockRadius : kAimRadius + 6.0f * std::sin(m_Time * 8.0f);
		float spin = m_Locked ? m_Time * 0.5f : m_Time * 2.5f;	// ロック前は速く回る

		// 4つのカギ括弧のような弧
		for (int q = 0; q < 4; q++)
		{
			float start = spin + q * (RibbonShapes::kPi * 0.5f) - 0.45f;
			RibbonShapes::Arc(m_Pts, p, right, up, radius, start, 0.9f, 8, RibbonShapes::MakeColor(col, 1.0f));
			Flush(cam, m_Locked ? 10.0f : 7.0f, m_Locked ? 1.8f : 1.3f, col);
		}

		// ロックした瞬間：外へ広がって消える輪
		if (m_LockFlash > 0.0f)
		{
			RibbonShapes::Arc(m_Pts, p, right, up, kLockRadius + (1.0f - m_LockFlash) * 90.0f, 0.0f, kTwoPi, 32,
				RibbonShapes::MakeColor(col, m_LockFlash));
			Flush(cam, 12.0f, 2.0f, col);
		}
	}

	Renderer::SetBlendState(BS_NONE);
}
