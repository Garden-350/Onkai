#include "SpecialGaugeHud.h"
#include "SpecialMove.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

void SpecialGaugeHud::Init()
{
	m_Frame.Init();       m_Frame.SetTexture("assets/UI/sp_gauge_frame.png");
	m_FillBlue.Init();    m_FillBlue.SetTexture("assets/UI/sp_fill_blue.png");
	m_FillGold.Init();    m_FillGold.SetTexture("assets/UI/sp_fill_gold.png");
	m_Tip.Init();         m_Tip.SetTexture("assets/UI/sp_fill_tip.png");
	m_Glow.Init();        m_Glow.SetTexture("assets/UI/sp_gauge_glow.png");
	m_Label.Init();       m_Label.SetTexture("assets/UI/sp_label.png");
	m_LabelCharge.Init(); m_LabelCharge.SetTexture("assets/UI/sp_label_charge.png");
	m_KeySpace.Init();    m_KeySpace.SetTexture("assets/UI/sp_key_space.png");
	m_Shown = 0.0f;
	m_Gold = false;
	m_Ready = false;
	m_ReadyPop = 0.0f;
}

void SpecialGaugeHud::Update(float dt, const SpecialMove& sp)
{
	m_Time += dt;

	bool gold = (sp.GetState() != SpecialMove::State::Gauge);
	if (gold != m_Gold)
	{
		m_Gold = gold;
		m_Shown = 0.0f;		// 青 → 金、金 → 青 に切り替わる時は 0 から伸ばし直す
	}

	float target = gold ? sp.GetChargeRatio() : sp.GetGauge();
	m_Shown += (target - m_Shown) * std::min(1.0f, kFollow * dt);

	// 本当に満タンの時だけ「発動できる」表示にする（デバッグ設定の kDebugAlwaysReady は見ない）
	bool ready = (sp.GetState() == SpecialMove::State::Gauge) && (sp.GetGauge() >= 1.0f);
	if (ready && !m_Ready) m_ReadyPop = 1.0f;
	m_Ready = ready;
	m_ReadyPop = std::max(0.0f, m_ReadyPop - dt * 3.0f);
}

void SpecialGaugeHud::DrawQuad(Texture2D& t, Camera* cam, float x, float y, float w, float h, float alpha)
{
	if (alpha <= 0.003f || w < 0.5f || h < 0.5f) return;
	t.SetPosition(x, y, 0.0f);
	t.SetScale(w, h, 0.0f);
	t.SetAlpha(alpha);
	t.Draw(cam);
}

void SpecialGaugeHud::Draw(Camera* cam)
{
	const float innerW = kInnerRight - kInnerLeft;
	const float innerH = kInnerBottom - kInnerTop;
	const float left = m_X - kFrameW * 0.5f + kInnerLeft;						// 塗りの左端
	const float cy = m_Y + (kFrameH * 0.5f - (kInnerTop + kInnerBottom) * 0.5f);	// 塗りの縦の中心（画像は下向きに数えるので符号に注意）

	// 満タン：枠の後ろで金の光が脈打つ
	if (m_Ready)
	{
		float pulse = 0.55f + 0.35f * std::sin(m_Time * 6.0f) + 0.5f * m_ReadyPop;
		float s = 1.0f + 0.08f * m_ReadyPop;
		DrawQuad(m_Glow, cam, m_X, m_Y, 600.0f * s, 160.0f * s, std::min(1.0f, pulse));
	}

	DrawQuad(m_Frame, cam, m_X, m_Y, kFrameW, kFrameH, 1.0f);

	// 塗り（左端を固定して、右へ伸ばす）
	float r = std::clamp(m_Shown, 0.0f, 1.0f);
	float w = innerW * r;
	if (w >= 1.0f)
	{
		Texture2D& fill = m_Gold ? m_FillGold : m_FillBlue;
		DrawQuad(fill, cam, left + w * 0.5f, cy, w, innerH, 1.0f);
		DrawQuad(m_Tip, cam, left + w, cy, 44.0f, 44.0f, 0.9f);			// 先端の光
	}

	// ラベル（枠の左上）
	if (m_Gold)
		DrawQuad(m_LabelCharge, cam, left + kChargeLabelW * 0.5f, m_Y + 44.0f, kChargeLabelW, kChargeLabelH, 1.0f);
	else
		DrawQuad(m_Label, cam, left + kLabelW * 0.5f, m_Y + 44.0f, kLabelW, kLabelH, 1.0f);

	// 満タン：右に SPACE キー
	if (m_Ready)
	{
		float s = 1.0f + 0.25f * m_ReadyPop;
		float a = 0.85f + 0.15f * std::sin(m_Time * 6.0f);
		DrawQuad(m_KeySpace, cam, m_X + kFrameW * 0.5f + kKeyW * 0.5f - 4.0f, m_Y, kKeyW * s, kKeyH * s, a);
	}
}
