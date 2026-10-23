#pragma once
#include "Texture2D.h"

class SpecialMove;

// ============================================================
// SpecialGaugeHud : 必殺技ゲージ（フラスコに魔力の液体が溜まっていく。表示だけ）
//   溜め中     … 青緑の液体が溜まっていく
//   満タン     … 液体が金に変わり、後ろが金に光って、下に「GO!」が出る
//   発動中     … 一度空になり、取ったチャージノーツの割合だけ金の液体が溜まり直す
//   発射       … 液体が減って空になる
//   液面の波は、8コマの波の位相を時間で切り替え、隣のコマと重ねてなめらかにしている。
//   量（12段階）も、隣の段と重ねてなめらかにしている。
//
//   使う画像（assets/UI/hurasuko/）：
//     flask_back / flask_liquid_teal / flask_liquid_gold / flask_front / flask_glow / flask_go（すべて .png）
// ============================================================
class SpecialGaugeHud
{
public:
	void Init();
	void SetPosition(float x, float y) { m_X = x; m_Y = y; }	// フラスコの中心（画面中心が原点、上がプラス）
	void SetScale(float s) { m_Scale = s; }
	void Update(float dt, const SpecialMove& sp);
	void Draw(Camera* cam);

private:
	// 画像の作り（flask2.py と合わせる）
	static constexpr float kCellW = 152.0f, kCellH = 288.0f;	// 1コマの大きさ
	static constexpr int   kPhases = 8;		// 液体シートの横のコマ数（波の位相）
	static constexpr int   kLevels = 12;	// 液体シートの縦のコマ数（量。上から 1/12 … 12/12）
	static constexpr float kGlowW = 212.0f, kGlowH = 348.0f;
	static constexpr float kGoW = 200.0f, kGoH = 120.0f;

	// 動きの調整
	static constexpr float kWavePeriod = 1.2f;	// 波が1周する秒数
	static constexpr float kFollow = 8.0f;		// 液面が目標の量に追いつく速さ
	static constexpr float kGoldSpeed = 4.0f;	// 青緑 → 金 に変わる速さ（1/秒）

	static void DrawQuad(Texture2D& t, Camera* cam, float x, float y, float w, float h, float alpha);
	void DrawLiquid(int color, Camera* cam, float level, float alpha);	// color: 0=青緑 1=金

	Texture2D m_Back, m_Front, m_Glow, m_Go;
	Texture2D m_Liquid[2][3];	// [色][重ねる3枚]（同じ画像を UV を変えて3回描くので、別々に持つ）

	float m_X = -555.0f;
	float m_Y = 120.0f;
	float m_Scale = 1.0f;
	float m_Shown = 0.0f;		// 表示している量 0～1（なめらかに追いかける）
	float m_GoldMix = 0.0f;		// 0=青緑 1=金
	bool  m_Ready = false;		// 満タンで発動できるか
	float m_ReadyPop = 0.0f;	// 満タンになった瞬間 1 → 0（GO! がポンと出る）
	float m_Time = 0.0f;
};
