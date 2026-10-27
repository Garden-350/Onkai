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
	void SetScale(float s) { m_Scale = s; }

private:
	//画像の作り
	static constexpr float kCellW = 152.0f, kCellH = 288.0f;	//1コマの大きさ
	static constexpr float kGlowW = 212.0f, kGlowH = 348.0f;
	static constexpr float kGoW = 160.0f, kGoH = 160.0f;	//Goの大きさ
	static constexpr int kPhases = 8;	//液体シートの横のコマ数
	static constexpr int kLevels = 12;	//液体シートの縦のコマ数

	//動き
	static constexpr float kWavePeriod = 1.2f;	//波が一周する秒数
	static constexpr float kFollow = 8.0f;		//液体が目標の量に追いつく速さ
	static constexpr float kGoldSpeed = 4.0f;	//青から金に変わる速さ

	static void DrawQuad(Texture2D& t, Camera* cam, float x, float y, float w, float h, float alpha);
	void DrawLiquid(int color, Camera* cam, float level, float alpha);	//color0青　１金

	Texture2D m_Back, m_Front, m_Glow, m_Go;
	Texture2D m_Liquid[2][3];	//色、重ねるやつ	同じ画像をUVを変えるために３回書くので別々に

	float m_X = 0.0f;
	float m_Y = 0.0f;
	float m_Scale = 1.0f;
	float m_Shown = 0.0f;		// 表示している値（なめらかに追いかける）
	float m_GoldMix = 0.0f;	//0青1金
	bool m_Ready = false;
	float m_ReadyPop = 0.0f;	//満タンになったら出る
	float m_Time = 0.0f;
};
