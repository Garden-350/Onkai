#include "SpecialGaugeHud.h"
#include "SpecialMove.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

void SpecialGaugeHud::Init()
{
	m_Back.Init();  m_Back.SetTexture("assets/UI/hurasuko/flask_back.png");
	m_Front.Init(); m_Front.SetTexture("assets/UI/hurasuko/flask_front.png");
	m_Glow.Init();  m_Glow.SetTexture("assets/UI/hurasuko/flask_glow.png");
	m_Go.Init();    m_Go.SetTexture("assets/UI/hurasuko/Go.png");
	const char* liquidPaths[2] = { "assets/UI/hurasuko/flask_liquid_teal.png",
									"assets/UI/hurasuko/flask_liquid_gold.png" };

	for (int c = 0; c < 2; c++)
	{
		for (int k = 0; k < 3; k++)
		{
			m_Liquid[c][k].Init();
			m_Liquid[c][k].SetTexture(liquidPaths[c]);
		}
	}

	m_Shown = 0.0f;
	m_GoldMix = 0.0f;
	m_Ready = false;
	m_ReadyPop = 0.0f;
}

void SpecialGaugeHud::Update(float dt, const SpecialMove& sp)
{
	m_Time += dt;

	//目標の量　発射中は空
	float target = 0.0f;
	switch (sp.GetState())
	{
	case SpecialMove::State::Gauge:	target = sp.GetGauge(); break;
	case SpecialMove::State::Waiting:
	case SpecialMove::State::Charging: target = sp.GetChargeRatio(); break;
	case SpecialMove::State::Firing:	target = 0.0f; break;
	}
	m_Shown += (target - m_Shown) * std::min(1.0f, kFollow * dt);

	//満タンの時だけ発動　デバッグは無視
	bool ready = (sp.GetState() == SpecialMove::State::Gauge) && (sp.GetGauge() >= 1.0f);
	if (ready && !m_Ready) m_ReadyPop = 1.0f;
	m_Ready = ready;
	m_ReadyPop = std::max(0.0f, m_ReadyPop - dt * 3.0f);

	//色
	bool gold = ready || (sp.GetState() != SpecialMove::State::Gauge);
	float goldTarget = gold ? 1.0f : 0.0f;
	if (m_GoldMix < goldTarget) m_GoldMix = std::min(goldTarget, m_GoldMix + kGoldSpeed * dt);
	else                        m_GoldMix = std::max(goldTarget, m_GoldMix - kGoldSpeed * dt);
}

void SpecialGaugeHud::DrawQuad(Texture2D& t, Camera* cam, float x, float y, float w, float h, float alpha)
{
	if (alpha <= 0.003f || w < 0.5f || h < 0.5f) return;
	t.SetPosition(x, y, 0.0f);
	t.SetScale(w, h, 0.0f);
	t.SetAlpha(alpha);
	t.Draw(cam);
}

//量と波の位相を隣のコマと重ねてなめらかに描く
void SpecialGaugeHud::DrawLiquid(int color, Camera* cam, float level, float alpha)
{
	if (alpha < 0.003f || level <= 0.001f) return;

	float f = std::clamp(level, 0.0f, 1.0f) * kLevels;
	int i = (int)std::floor(f);		//0~16
	float fl = f - (float)i;

	float phase = m_Time / kWavePeriod * kPhases;
	int k = (int)std::floor(phase) % kPhases;
	float fp = phase - std::floor(phase);	//次の位相への割合
	int k2 = (k + 1) % kPhases;

	int row = std::clamp(i, 1, kLevels);	//シートの段
	float baseA = (i >= 1) ? alpha : alpha * fl;	//最初の段に満たない時は一段目を薄く

	const float w = kCellW * m_Scale, h = kCellH * m_Scale;
	Texture2D* t = m_Liquid[color];

	//1枚目　今の段と位相
	t[0].SetUV((float)(k + 1), (float)row, (float)kPhases, (float)kLevels);
	DrawQuad(t[0], cam, m_X, m_Y, w, h, baseA);
	//2枚目　波
	t[1].SetUV((float)(k2 + 1), (float)row, (float)kPhases, (float)kLevels);
	DrawQuad(t[1], cam, m_X, m_Y, w, h, baseA * fp);
	//3枚目
	if (i >= 1 && i < kLevels)
	{
		t[2].SetUV((float)(k + 1), (float)(i + 1), (float)kPhases, (float)kLevels);
		DrawQuad(t[2], cam, m_X, m_Y, w, h, alpha * fl);
	}
}

void SpecialGaugeHud::Draw(Camera* cam)
{
	const float s = m_Scale;

	//満タン　金
	if (m_Ready)
	{
		float pulse = std::min(1.0f, 0.6f + 0.3f * std::sin(m_Time * 6.0f) + 0.5f * m_ReadyPop);
		DrawQuad(m_Glow, cam, m_X, m_Y, kGlowW * s, kGlowH * s, pulse);
	}
	DrawQuad(m_Back, cam, m_X, m_Y, kCellW * s, kCellH * s, 1.0f);

	//液体
	DrawLiquid(0, cam, m_Shown, 1.0f - m_GoldMix);
	DrawLiquid(1, cam, m_Shown, m_GoldMix);
	DrawQuad(m_Front, cam, m_X, m_Y, kCellW * s, kCellH * s, 1.0f);

	if (m_Ready)
	{
		float gs = s * (1.0f + 0.35f * m_ReadyPop);
		float a = 0.85f + 0.15f * std::sin(m_Time * 6.0f);
		DrawQuad(m_Go, cam, m_X, m_Y - kCellH * 0.5f * s - 40.0f * s, kGoW * gs, kGoH * gs, a);
	}

}
