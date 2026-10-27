#include "WeakPoint.h"
#include "RibbonShapes.h"
#include "Camera.h"
#include "Renderer.h"
#include "Application.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void WeakPoint::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(64);

	// キーの画像（判定円のアイコンと同じもの）
	const char* keyPaths[6] = {
		"assets/texture/S.png", "assets/texture/D.png", "assets/texture/F.png",
		"assets/texture/J.png", "assets/texture/K.png", "assets/texture/L.png",
	};
	for (int i = 0; i < 6; i++)
	{
		m_KeyIcon[i].Init();
		m_KeyIcon[i].SetTexture(keyPaths[i]);
		m_LaneColor[i] = Vector3(1.0f, 1.0f, 1.0f);
	}

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
	m_OffX = 0.0f;
	m_OffY = 0.0f;
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

// 弱点の場所：敵の中心から、カメラから見た面の上で (m_OffX, m_OffY) だけずらした所
Vector3 WeakPoint::GetWeakPos() const
{
	Vector3 right, up, toCam;
	Basis(right, up, toCam);
	Vector3 c = m_Center + up * kPosLift + toCam * kPosFront;
	if (m_WeakLane < 0) return c;
	return c + right * m_OffX + up * m_OffY;
}

// 弱点のキーと場所を選び直す（どちらも前と同じにはしない）
void WeakPoint::PickNew()
{
	std::uniform_int_distribution<int> laneDist(0, 5);
	int lane = laneDist(m_Rng);
	if (m_WeakLane >= 0)
	{
		while (lane == m_WeakLane) lane = laneDist(m_Rng);
	}

	// 場所：円の中にまんべんなく散らす（半径は平方根を取ると、中心に偏らない）
	std::uniform_real_distribution<float> u01(0.0f, 1.0f);
	float bestX = 0.0f, bestY = 0.0f;
	for (int tries = 0; tries < 12; tries++)
	{
		float a = u01(m_Rng) * 2.0f * RibbonShapes::kPi;
		float r2min = kPosRadiusMin * kPosRadiusMin;
		float r2max = kPosRadiusMax * kPosRadiusMax;
		float r = std::sqrt(r2min + (r2max - r2min) * u01(m_Rng));
		float x = std::sin(a) * r;
		float y = std::cos(a) * r;
		bestX = x;
		bestY = y;
		float dx = x - m_OffX, dy = y - m_OffY;
		if (m_WeakLane < 0 || dx * dx + dy * dy >= kPosMinMove * kPosMinMove) break;	// 前の場所から十分離れた
	}

	m_WeakLane = lane;
	m_OffX = bestX;
	m_OffY = bestY;
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

	if (m_Mode != Mode::Normal) return;	// 必殺技の間は弱点を動かさない
	if (musicTime < 0.0f) return;		// カウントイン中は弱点なし

	if (m_WeakLane < 0)
	{
		// 曲が始まった：最初の弱点を選ぶ
		PickNew();
		m_PhaseStart = musicTime;
		m_PhaseEnd = musicTime + m_Interval;
	}
	else if (musicTime >= m_PhaseEnd)
	{
		// 時間が来た：次の弱点へ
		PickNew();
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
	PickNew();					// 必殺技用に選び直して、撃つまで固定
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
	PickNew();
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

// 4つのカギ括弧のような狙いの枠
void WeakPoint::DrawBrackets(Camera* cam, const Vector3& center, float radius, float spin, const Vector3& color, bool locked)
{
	Vector3 right, up;
	RibbonShapes::CameraBasis(center, m_CamPos, right, up);
	for (int q = 0; q < 4; q++)
	{
		float start = spin + q * (RibbonShapes::kPi * 0.5f) - 0.45f;
		RibbonShapes::Arc(m_Pts, center, right, up, radius, start, 0.9f, 8, RibbonShapes::MakeColor(color, 1.0f));
		Flush(cam, locked ? 10.0f : 7.0f, locked ? 1.8f : 1.3f, color);
	}
	if (m_LockFlash > 0.0f)	// ロックした瞬間：外へ広がって消える輪
	{
		RibbonShapes::Arc(m_Pts, center, right, up, radius + (1.0f - m_LockFlash) * 90.0f, 0.0f, 2.0f * RibbonShapes::kPi, 32,
			RibbonShapes::MakeColor(color, m_LockFlash));
		Flush(cam, 12.0f, 2.0f, color);
	}
}

// ワールド座標 → Texture2D の座標（画面の真ん中が 0,0、上がプラス）
Vector3 WeakPoint::ToScreen(Camera* cam, const Vector3& world)
{
	float w = (float)Application::GetWidth();
	float h = (float)Application::GetHeight();
	DirectX::XMVECTOR p = DirectX::XMVector3Project(
		DirectX::XMLoadFloat3(&world), 0.0f, 0.0f, w, h, 0.0f, 1.0f,
		cam->GetProjectionMatrix(), cam->GetViewMatrix(), DirectX::XMMatrixIdentity());
	Vector3 s;
	DirectX::XMStoreFloat3(&s, p);
	return Vector3(s.x - w * 0.5f, h * 0.5f - s.y, 0.0f);
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
	Vector3 right, up;
	m_Pts.clear();

	const bool hasWeak = (m_WeakLane >= 0);
	const Vector3 weakPos = GetWeakPos();
	Vector3 weakScreen;
	if (hasWeak) weakScreen = ToScreen(cam, weakPos);	// 3D のカメラの間に計算しておく

	// ── 敵の上の弱点（六角形・残り時間の輪・変わった瞬間の輪）──
	if (hasWeak)
	{
		Vector3 col = m_LaneColor[m_WeakLane];
		RibbonShapes::CameraBasis(weakPos, m_CamPos, right, up);
		float size = 1.0f + kBeatKick * kick;	// 拍ごとに少し膨らむ

		RibbonShapes::Polygon(m_Pts, weakPos, right, up, kWeakRadius * size, 6, m_Time * 0.6f,
			RibbonShapes::MakeColor(col, 0.9f));
		Flush(cam, 7.0f, 1.2f + 0.6f * kick, col);

		DrawTimerRing(cam, weakPos, right, up, kTimerRadius, 6.0f, col, 1.0f);

		if (m_SwitchFlash > 0.0f)
		{
			RibbonShapes::Arc(m_Pts, weakPos, right, up, kWeakRadius + (1.0f - m_SwitchFlash) * 90.0f, 0.0f, kTwoPi, 32,
				RibbonShapes::MakeColor(col, m_SwitchFlash));
			Flush(cam, 10.0f, 1.5f, col);
		}

		// ── 弱点のレーンの判定円を、六角形で囲む ──
		if (kShowLaneMarker && m_HasAnchor[m_WeakLane])
		{
			Vector3 q = m_Anchor[m_WeakLane];
			RibbonShapes::CameraBasis(q, m_CamPos, right, up);
			Vector3 mc = col * 0.5f + Vector3(1.0f, 0.85f, 0.4f) * 0.5f;	// レーン色と金の中間
			RibbonShapes::Polygon(m_Pts, q, right, up, kLaneMarkerRadius * (1.0f + 0.08f * kick), 6, m_Time * 0.6f,
				RibbonShapes::MakeColor(mc, kLaneMarkerAlpha));
			Flush(cam, 6.0f, 1.2f + 0.4f * kick, col);
			DrawTimerRing(cam, q, right, up, kLaneMarkerRadius + 14.0f, 4.0f, col, kLaneMarkerAlpha);
		}
	}

	// ── 狙いの枠（必殺技のチャージ中だけ）──
	if (m_Mode == Mode::Aiming)
	{
		bool match = (m_Aim == m_WeakLane);
		bool hint = kShowMatchHint && match;
		Vector3 col = m_Locked ? Vector3(1.0f, 0.95f, 0.7f)
			: (hint ? Vector3(1.0f, 0.8f, 0.25f) : Vector3(0.75f, 0.85f, 1.0f));
		float spin = m_Locked ? m_Time * 0.4f : m_Time * 1.2f;
		float radius = m_Locked ? kLockRadius : kAimRadius;

		// 判定円の枠：今狙っているレーン（手元で分かるように）
		if (m_HasAnchor[m_Aim]) DrawBrackets(cam, m_Anchor[m_Aim], radius * 1.15f, spin, col, m_Locked);
		// 敵の弱点の枠：狙いが弱点のキーと合っている時だけ（ヒントを出す設定の時）
		if (hint && hasWeak) DrawBrackets(cam, weakPos, radius, spin, col, m_Locked);
	}

	// ── キーの画像（2D。いちばん手前に描く）──
	if (hasWeak)
	{
		float pop = 1.0f + kKeyIconPop * m_SwitchFlash * m_SwitchFlash;	// 出た瞬間は大きく、すぐ縮む
		float size = kKeyIconSize * pop * (1.0f + 0.5f * kBeatKick * kick);
		Texture2D& icon = m_KeyIcon[m_WeakLane];
		Renderer::SetBlendState(BS_ALPHABLEND);
		icon.SetPosition(weakScreen.x, weakScreen.y, 0.0f);
		icon.SetScale(size, size, 0.0f);
		icon.SetAlpha(1.0f);
		icon.Draw(cam);		// Texture2D::Draw の中で 2D のカメラに切り替わる
		cam->SetCamera(0);	// 3D のカメラに戻しておく
	}

	Renderer::SetBlendState(BS_NONE);
}
