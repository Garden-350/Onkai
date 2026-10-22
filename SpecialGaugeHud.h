#pragma once
#include "Texture2D.h"

class SpecialMove;

//SpecialGaugeHud : 画面左上の必殺技ゲージ（表示だけ。値は SpecialMove から読む）
class SpecialGaugeHud
{
public:
	void Init();
	void SetPosition(float x, float y) { m_X = x; m_Y = y; }	// 枠の中心（画面中心が原点、上がプラス）
	void Update(float dt, const SpecialMove& sp);
	void Draw(Camera* cam);

private:
	// 画像のサイズと、枠の中で塗る場所（画像のピクセル）
	static constexpr float kFrameW = 520.0f, kFrameH = 80.0f;
	static constexpr float kInnerLeft = 24.0f, kInnerRight = 496.0f;
	static constexpr float kInnerTop = 26.0f, kInnerBottom = 54.0f;
	static constexpr float kLabelW = 144.0f, kLabelH = 73.0f;
	static constexpr float kChargeLabelW = 169.0f, kChargeLabelH = 58.0f;
	static constexpr float kKeyW = 211.0f, kKeyH = 102.0f;
	static constexpr float kFollow = 10.0f;		// 表示の値が本当の値に追いつく速さ（大きいほど速い）

	static void DrawQuad(Texture2D& t, Camera* cam, float x, float y, float w, float h, float alpha);

	Texture2D m_Frame, m_FillBlue, m_FillGold, m_Tip, m_Glow, m_Label, m_LabelCharge, m_KeySpace;
	float m_X = -340.0f;
	float m_Y = 290.0f;
	float m_Shown = 0.0f;		// 表示している値（なめらかに追いかける）
	bool  m_Gold = false;		// 発動中（金の表示）か
	bool  m_Ready = false;		// 満タンで発動できるか
	float m_ReadyPop = 0.0f;	// 満タンになった瞬間 1 → 0
	float m_Time = 0.0f;
};
