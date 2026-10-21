#include "HexHpGauge.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

void HexHpGauge::Init()
{
	m_Frame.Init();
	m_Frame.SetTexture("assets/UI/hp_hex_frame.png");
	m_Trail.Init();
	m_Trail.SetTexture("assets/UI/hp_hex_trail_sheet.png");
	m_Fill.Init();
	m_Fill.SetTexture("assets/UI/hp_hex_fill_sheet.png");
	SetStep(m_Trail, 1.0f);
	SetStep(m_Fill, 1.0f);
}

// 割合（0～1）を、シートのコマに変換してUVをセットする
//   コマ0 = 満タン、コマ35 = 残り1/36。0より大きければ最低1コマは見せる
void HexHpGauge::SetStep(Texture2D& tex, float ratio)
{
	int remain = (int)std::ceil(std::clamp(ratio, 0.0f, 1.0f) * kSteps);	// 残っている段数（0～36）
	int i = kSteps - std::max(remain, 1);									// シートのコマ番号
	tex.SetUV((float)(i % kCols + 1), (float)(i / kCols + 1), (float)kCols, (float)(kSteps / kCols));	// 番号は1から
}

void HexHpGauge::SetRatio(float ratio)
{
	ratio = std::clamp(ratio, 0.0f, 1.0f);
	if (ratio < m_Ratio - 0.0001f)		// 減った瞬間だけ
	{
		m_TrailWait = kTrailDelay;		// 残像はしばらくその場に残す
		m_HitPop = 1.0f;
	}
	m_Ratio = ratio;
	if (m_TrailRatio < ratio) m_TrailRatio = ratio;	// 回復した時は残像も合わせる
}

void HexHpGauge::Update(float dt)
{
	if (m_TrailWait > 0.0f)
		m_TrailWait -= dt;
	else
		m_TrailRatio = std::max(m_Ratio, m_TrailRatio - kTrailSpeed * dt);	// 遅れて追いかける
	m_HitPop = std::max(0.0f, m_HitPop - dt * 5.0f);
}

void HexHpGauge::Draw(Camera* cam)
{
	float s = m_Size * (1.0f + 0.06f * m_HitPop);	// 当たった瞬間ちょっと膨らむ
	Texture2D* layers[3] = { &m_Frame, &m_Trail, &m_Fill };
	for (Texture2D* t : layers)
	{
		t->SetPosition(m_X, m_Y, 0.0f);
		t->SetScale(s, s, 0.0f);
	}

	m_Frame.Draw(cam);								// 枠（空の溝）
	if (m_TrailRatio > m_Ratio + 0.001f)			// 白い残像（減った直後だけ）
	{
		SetStep(m_Trail, m_TrailRatio);
		m_Trail.Draw(cam);
	}
	if (m_Ratio > 0.0f)								// 赤いゲージ本体
	{
		SetStep(m_Fill, m_Ratio);
		m_Fill.Draw(cam);
	}
}
