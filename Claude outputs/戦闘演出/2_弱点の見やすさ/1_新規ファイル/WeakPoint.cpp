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

void WeakPoint::SetLaneAnchor(int lane, const Vector3& pos)
{
	if (lane < 0 || lane >= 6) return;
	m_Anchor[lane] = pos;
	m_HasAnchor[lane] = true;
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

// 敵から見て、カメラの方を向いた面の「右」「上」と、カメラへの向き
void WeakPoint::Basis(Vector3& right, Vector3& up, Vector3& toCam) const
{
	Vector3 camPos = m_HasCam ? m_CamPos : m_Center + Vector3(0.0f, 0.0f, -1000.0f);	// 最初の描画の前は真正面から
	RibbonShapes::CameraBasis(m_Center, camPos, right, up);
	toCam = camPos - m_Center;
	if (toCam.LengthSquared() < 0.0001f) toCam = Vector3(0.0f, 0.0f, -1.0f);
	toCam.Normalize();
}

// 6つの部位の位置：敵のまわりの「冠」の上に、画面の左から S D F J K L の順に並べる
Vector3 WeakPoint::GetPoint(int lane) const
{
	lane = std::clamp(lane, 0, 5);
	Vector3 right, up, toCam;
	Basis(right, up, toCam);

	int idx = m_Flip ? 5 - lane : lane;		// 画面の左から何番目か
	float t = (idx - 2.5f) / 2.5f;			// -1（左端）～ +1（右端）
	float a = t * kHaloSpanDeg * RibbonShapes::kPi / 180.0f;	// 真上が0、右がプラス

	Vector3 c = m_Center + up * kHaloLift + toCam * kHaloFront;
	return c + (right * std::sin(a) + up * std::cos(a)) * kHaloRadius;
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

// 拍に合わせた脈：beatsPerPulse 拍ごとに 1 になり、sharpness が大きいほど早く 0 に戻る
float WeakPoint::BeatPulse(float beatsPerPulse, float sharpness) const
{
	float len = m_BeatLen * beatsPerPulse;
	if (len <= 0.0001f || m_Now < 0.0f) return 0.0f;
	float phase = std::fmod(m_Now, len) / len;	// 0～1
	return std::exp(-phase * sharpness);
}

float WeakPoint::Remain() const
{
	float len = m_PhaseEnd - m_PhaseStart;
	return (len > 0.0001f) ? std::clamp((m_PhaseEnd - m_Now) / len, 0.0f, 1.0f) : 0.0f;
}

void WeakPoint::Update(float deltaTime, float musicTime, float bpm, int beatsPerBar, const Vector3& enemyCenter)
{
	m_Time += deltaTime;
	m_Now = musicTime;
	m_Center = enemyCenter;
	m_HasCenter = true;
	m_LockFlash = std::max(0.0f, m_LockFlash - deltaTime * 3.0f);
	m_SwitchFlash = std::max(0.0f, m_SwitchFlash - deltaTime * 2.5f);

	m_BeatLen = (bpm > 0.0f) ? 60.0f / bpm : 0.5f;
	m_Interval = m_BeatLen * (float)std::max(1, beatsPerBar) * (float)kBarsPerSwitch;

	// 判定円が画面の右から S … と並んでいたら、冠の並びも左右を入れ替える
	if (m_HasAnchor[0] && m_HasAnchor[5])
	{
		Vector3 right, up, toCam;
		Basis(right, up, toCam);
		m_Flip = (m_Anchor[0].Dot(right) > m_Anchor[5].Dot(right));
	}

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

// 残り時間の輪：真上から時計回りに、残っている分だけ。残りわずかで赤くなり、半拍ごとに脈打つ
void WeakPoint::DrawTimerRing(Camera* cam, const Vector3& center, const Vector3& right, const Vector3& up,
	float radius, float width, const Vector3& laneColor, float alpha)
{
	float remain = Remain();
	if (remain <= 0.001f) return;

	Vector3 col = laneColor * 0.3f + Vector3(0.7f, 0.7f, 0.7f);	// 白寄りのレーン色
	float a = alpha;
	if (remain < kWarnRatio)
	{
		float w = 1.0f - remain / kWarnRatio;	// 0 → 1
		col = col * (1.0f - w) + Vector3(1.0f, 0.25f, 0.2f) * w;	// 赤へ
		a *= 0.55f + 0.45f * BeatPulse(0.5f, 5.0f);	// 半拍ごとに脈打つ
	}

	int segs = std::max(4, (int)(48.0f * remain));
	RibbonShapes::Arc(m_Pts, center, right, up, radius, 0.0f, 2.0f * RibbonShapes::kPi * remain, segs,
		RibbonShapes::MakeColor(col, a));
	Flush(cam, width, 1.2f, laneColor);
}

void WeakPoint::Draw(Camera* cam)
{
	if (!m_HasCenter) return;

	m_CamPos = cam->GetPosition();
	m_HasCam = true;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);
	const float kTwoPi = 2.0f * RibbonShapes::kPi;
	const float kick = BeatPulse(1.0f, 6.0f);	// 拍の頭で 1
	Vector3 right, up, toCam;
	m_Pts.clear();

	// ── 弱点でない部位の小さい印（静かに置いておくだけ）──
	if (kMarkAlpha > 0.0f)
	{
		for (int lane = 0; lane < 6; lane++)
		{
			if (lane == m_WeakLane) continue;
			Vector3 p = GetPoint(lane);
			RibbonShapes::CameraBasis(p, m_CamPos, right, up);
			RibbonShapes::Arc(m_Pts, p, right, up, kMarkRadius, 0.0f, kTwoPi, 16,
				RibbonShapes::MakeColor(m_LaneColor[lane], kMarkAlpha));
			Flush(cam, 4.0f, 0.8f, m_LaneColor[lane]);
		}
	}

	// ── 弱点 ──
	if (m_WeakLane >= 0)
	{
		Vector3 p = GetPoint(m_WeakLane);
		Vector3 col = m_LaneColor[m_WeakLane];
		RibbonShapes::CameraBasis(p, m_CamPos, right, up);
		float size = 1.0f + kBeatKick * kick;	// 拍ごとに少し膨らむ

		// 六角形（ゆっくり回る）
		RibbonShapes::Polygon(m_Pts, p, right, up, kWeakRadius * size, 6, m_Time * 0.6f,
			RibbonShapes::MakeColor(col, 1.0f));
		Flush(cam, 9.0f, 1.4f + 0.6f * kick, col);

		// 真ん中の点
		RibbonShapes::Arc(m_Pts, p, right, up, 8.0f, 0.0f, kTwoPi, 16, Color(1.0f, 1.0f, 1.0f, 1.0f));
		Flush(cam, 10.0f, 1.8f, col);

		// 残り時間の輪
		DrawTimerRing(cam, p, right, up, kTimerRadius, 6.0f, col, 1.0f);

		// 移った瞬間：外へ広がって消える輪
		if (m_SwitchFlash > 0.0f)
		{
			RibbonShapes::Arc(m_Pts, p, right, up, kWeakRadius + (1.0f - m_SwitchFlash) * 80.0f, 0.0f, kTwoPi, 32,
				RibbonShapes::MakeColor(col, m_SwitchFlash));
			Flush(cam, 10.0f, 1.5f, col);
		}

		// ── 弱点のレーンの判定円を、六角形で囲む ──
		if (kShowLaneMarker && m_HasAnchor[m_WeakLane])
		{
			Vector3 q = m_Anchor[m_WeakLane];
			RibbonShapes::CameraBasis(q, m_CamPos, right, up);
			Vector3 gold(1.0f, 0.85f, 0.4f);
			Vector3 mc = col * 0.5f + gold * 0.5f;
			RibbonShapes::Polygon(m_Pts, q, right, up, kLaneMarkerRadius * (1.0f + 0.08f * kick), 6, m_Time * 0.6f,
				RibbonShapes::MakeColor(mc, kLaneMarkerAlpha));
			Flush(cam, 6.0f, 1.2f + 0.4f * kick, col);
			DrawTimerRing(cam, q, right, up, kLaneMarkerRadius + 14.0f, 4.0f, col, kLaneMarkerAlpha);

			if (m_SwitchFlash > 0.0f)
			{
				RibbonShapes::Arc(m_Pts, q, right, up, kLaneMarkerRadius + (1.0f - m_SwitchFlash) * 60.0f, 0.0f, kTwoPi, 32,
					RibbonShapes::MakeColor(mc, m_SwitchFlash));
				Flush(cam, 8.0f, 1.4f, col);
			}
		}
	}

	// ── 狙いの枠（必殺技のチャージ中だけ）──
	if (m_Mode == Mode::Aiming)
	{
		bool match = kShowMatchHint && (m_Aim == m_WeakLane);
		Vector3 col = m_Locked ? Vector3(1.0f, 0.95f, 0.7f)
			: (match ? Vector3(1.0f, 0.8f, 0.25f) : Vector3(0.75f, 0.85f, 1.0f));
		float spin = m_Locked ? m_Time * 0.4f : m_Time * 1.2f;	// ロック前は少し速く回る

		// 敵の部位の枠と、判定円の枠（狙っているレーンが手元でも分かるように）
		Vector3 targets[2] = { GetPoint(m_Aim), m_Anchor[m_Aim] };
		float radii[2] = { m_Locked ? kLockRadius : kAimRadius,
			(m_Locked ? kLockRadius : kAimRadius) * 1.15f };
		int count = m_HasAnchor[m_Aim] ? 2 : 1;

		for (int n = 0; n < count; n++)
		{
			Vector3 p = targets[n];
			RibbonShapes::CameraBasis(p, m_CamPos, right, up);

			// 4つのカギ括弧のような弧
			for (int q = 0; q < 4; q++)
			{
				float start = spin + q * (RibbonShapes::kPi * 0.5f) - 0.45f;
				RibbonShapes::Arc(m_Pts, p, right, up, radii[n], start, 0.9f, 8, RibbonShapes::MakeColor(col, 1.0f));
				Flush(cam, m_Locked ? 10.0f : 7.0f, m_Locked ? 1.8f : 1.3f, col);
			}

			// ロックした瞬間：外へ広がって消える輪
			if (m_LockFlash > 0.0f)
			{
				RibbonShapes::Arc(m_Pts, p, right, up, radii[n] + (1.0f - m_LockFlash) * 90.0f, 0.0f, kTwoPi, 32,
					RibbonShapes::MakeColor(col, m_LockFlash));
				Flush(cam, 12.0f, 2.0f, col);
			}
		}
	}

	Renderer::SetBlendState(BS_NONE);
}
