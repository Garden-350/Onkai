#include "ChargeAura.h"
#include "renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

void ChargeAura::Init()
{
	m_Bg.Init();       m_Bg.SetTexture("assets/UI/charge/charge_bg.png");
	m_Lines.Init();    m_Lines.SetTexture("assets/UI/charge/charge_lines_sheet.png");
	m_Burst.Init();    m_Burst.SetTexture("assets/UI/charge/charge_burst.png");
	m_Ring.Init();     m_Ring.SetTexture("assets/UI/charge/charge_ring.png");
	m_Star.Init();     m_Star.SetTexture("assets/UI/charge/charge_star.png");
	m_Spark.Init();    m_Spark.SetTexture("assets/UI/charge/charge_spark.png");
	m_Edge.Init();     m_Edge.SetTexture("assets/UI/charge/charge_edge.png");
	m_FlashTex.Init(); m_FlashTex.SetTexture("assets/UI/charge/charge_flash.png");
	m_Lines.SetUV(1.0f, 1.0f, 3.0f, 1.0f);
	Cancel();
}

void ChargeAura::Start()
{
	m_State = State::Charging;
	m_Progress = 0.0f;
	m_LastBeat = -1;		// 次の Update で 1拍目の輪が出る
	m_Flash = 0.6f;
	m_SparkAcc = 0.0f;
	m_StarAcc = 0.0f;
}

void ChargeAura::Release()
{
	if (m_State != State::Charging) return;
	m_State = State::Releasing;
	m_Flash = 1.0f;
}

void ChargeAura::Cancel()
{
	m_State = State::Idle;
	m_Intensity = 0.0f;
	m_Flash = 0.0f;
	m_Pulse = 0.0f;
	for (Ring& r : m_Rings) r.alive = false;
	for (Spark& s : m_Sparks) s.alive = false;
	for (Star& s : m_Stars) s.alive = false;
}

void ChargeAura::Update(float dt, float progress)
{
	if (m_State == State::Charging)
	{
		m_Progress = std::clamp(progress, 0.0f, 1.0f);
		m_Intensity = std::min(1.0f, m_Intensity + dt / kFadeIn);

		// 拍が変わった瞬間：脈打つ＋光の輪
		int beat = std::min((int)(m_Progress * kBeatsPerBar), kBeatsPerBar - 1);
		if (beat != m_LastBeat)
		{
			m_LastBeat = beat;
			m_Pulse = 1.0f;
			for (Ring& r : m_Rings)
				if (!r.alive) { r.alive = true; r.age = 0.0f; break; }
		}

		// 火の粉：進むほど多く・速く
		m_SparkAcc += dt * (30.0f + 90.0f * m_Progress);
		while (m_SparkAcc >= 1.0f)
		{
			m_SparkAcc -= 1.0f;
			for (Spark& s : m_Sparks)
			{
				if (s.alive) continue;
				s.alive = true;
				s.x = Rand(-640.0f, 640.0f);
				s.y = -380.0f;
				s.vy = Rand(250.0f, 350.0f) + 450.0f * m_Progress;
				s.size = Rand(22.0f, 48.0f);
				s.phase = Rand(0.0f, 6.28f);
				break;
			}
		}

		// きらめき：レーンの上は避けて、外側にキラッと
		m_StarAcc += dt * (16.0f + 20.0f * m_Progress);
		while (m_StarAcc >= 1.0f)
		{
			m_StarAcc -= 1.0f;
			for (Star& s : m_Stars)
			{
				if (s.alive) continue;
				for (int tries = 0; tries < 10; ++tries)
				{
					s.x = Rand(-620.0f, 620.0f);
					s.y = Rand(-200.0f, 330.0f);
					if (!(std::fabs(s.x) < kStarAvoidHalfW && s.y < kStarAvoidTopY)) break;
				}
				s.alive = true;
				s.age = 0.0f;
				s.life = Rand(0.35f, 0.6f);
				s.size = Rand(40.0f, 95.0f);
				s.rot = Rand(0.0f, 45.0f);
				break;
			}
		}
	}
	else if (m_State == State::Releasing)
	{
		m_Intensity = std::max(0.0f, m_Intensity - dt / kFadeOut);
		if (m_Intensity <= 0.0f) m_State = State::Idle;
	}

	m_BurstRot += kBurstSpin * dt * (1.0f + 1.5f * m_Pulse);	// 拍の瞬間だけ速く回る
	if (m_BurstRot > 360.0f) m_BurstRot -= 360.0f;
	m_Pulse *= std::exp(-dt * 8.0f);
	m_Flash = std::max(0.0f, m_Flash - dt * 4.0f);

	// 集中線のパターン切り替え（チラつき＝躍動感の正体）
	m_LineTimer -= dt;
	if (m_LineTimer <= 0.0f)
	{
		bool lastBeat = (m_State == State::Charging) && (m_Progress >= (kBeatsPerBar - 1) / (float)kBeatsPerBar);
		m_LineTimer = lastBeat ? kLineSwapLast : kLineSwap;
		m_LineFrame = (m_LineFrame + 1 + (int)Rand(0.0f, 1.99f)) % 3;
		m_LineRot = Rand(-4.0f, 4.0f);
	}

	for (Ring& r : m_Rings)
		if (r.alive && (r.age += dt) >= kRingLife) r.alive = false;

	for (Spark& s : m_Sparks)
	{
		if (!s.alive) continue;
		s.y += s.vy * dt;
		s.phase += dt * 6.0f;
		if (s.y > 400.0f) s.alive = false;
	}

	for (Star& s : m_Stars)
		if (s.alive && (s.age += dt) >= s.life) s.alive = false;
}

void ChargeAura::DrawQuad(Texture2D& t, Camera* cam, float x, float y, float w, float h, float rotDeg, float alpha)
{
	if (alpha <= 0.003f) return;
	t.SetPosition(x, y, 0.0f);
	t.SetScale(w, h, 0.0f);
	t.SetRotation(0.0f, 0.0f, rotDeg);		// 度で渡す
	t.SetAlpha(alpha);
	t.Draw(cam);
}

void ChargeAura::Draw(Camera* cam)
{
	if (!IsVisible()) return;
	const float it = m_Intensity;
	const float p = m_Progress;

	// 1. 金の下地
	DrawQuad(m_Bg, cam, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, it * kBgAlpha);

	// 2. 回る光の帯
	float bs = kBurstSize * (1.0f + 0.05f * m_Pulse);
	DrawQuad(m_Burst, cam, kFocusX, kFocusY, bs, bs, m_BurstRot, it * kBurstAlpha * (1.0f + 0.4f * m_Pulse));

	// 3. 拍の明滅（全体が一瞬明るく）
	DrawQuad(m_FlashTex, cam, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, it * kBeatFlash * m_Pulse);

	// 4. 集中線（拍で少し膨らむ）
	float ls = kLinesSize * (1.0f + 0.06f * m_Pulse);
	m_Lines.SetUV((float)(m_LineFrame + 1), 1.0f, 3.0f, 1.0f);
	DrawQuad(m_Lines, cam, kFocusX, kFocusY, ls, ls, m_LineRot, it * (0.25f + 0.25f * p));

	// 5. 光の輪（ease-out で広がりながら消える）
	for (const Ring& r : m_Rings)
	{
		if (!r.alive) continue;
		float u = r.age / kRingLife;
		float e = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
		float size = kRingFrom + (kRingTo - kRingFrom) * e;
		DrawQuad(m_Ring, cam, kFocusX, kFocusY, size, size, 0.0f, it * 0.9f * (1.0f - u));
	}

	// 6. 火の粉（ゆらゆら＋明滅）
	for (const Spark& s : m_Sparks)
	{
		if (!s.alive) continue;
		float a = it * (0.6f + 0.4f * std::sin(s.phase));
		DrawQuad(m_Spark, cam, s.x + std::sin(s.phase) * 10.0f, s.y, s.size, s.size, 0.0f, a);
	}

	// 7. きらめき（ふくらんで縮む＋少し回る）
	for (const Star& s : m_Stars)
	{
		if (!s.alive) continue;
		float u = s.age / s.life;
		float size = s.size * std::sin(3.14159f * u);
		DrawQuad(m_Star, cam, s.x, s.y, size, size, s.rot + 40.0f * u, it);
	}

	// 8. 縁の光
	DrawQuad(m_Edge, cam, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, it * std::min(1.0f, 0.3f + 0.3f * p + 0.25f * m_Pulse));

	// 9. フラッシュ
	DrawQuad(m_FlashTex, cam, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, m_Flash);

	// この板が深度を書いて、あとから描く敵やレーンを隠さないようにする
	Renderer::ClearCurrentDepth();
}
