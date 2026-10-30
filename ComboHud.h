#pragma once
#include "Texture2D.h"
#include "BarrierQuad.h"
#include <random>
#include <SimpleMath.h>

class Camera;

struct BarrierEffect
{
	bool active = false;
	float timer = 0.0f;
};

struct JudgeRankEffect
{
	bool active = false;
	float timer = 0.0f;
	float angle = 0.0f;
	int rank = 0;
};

// ============================================================
// ComboHud : コンボ数字・判定内訳(PERFECT/GREAT/GOOD/MISS)・
//   判定ランクのポップ演出・障壁演出(バリア)をまとめて担当するクラス。
//   トリガー(TriggerXxx)はChartObject::OnJudgeから、
//   減衰(Update)は毎フレーム、描画(Draw)はChartObject::Drawの後半から呼ぶ。
// ============================================================
class ComboHud
{
public:
	void Init();

	void TriggerRankPop(int laneIndex, int rank); // rank: 0=PERFECT,1=GREAT,2=GOOD
	void TriggerBarrier(int laneIndex);
	void TriggerComboPop();
	void TriggerFullCombo();

	void Update(float deltaTime);

	// judgeCircleScreenPos/judgeCirclePos3D/laneColors はいずれも
	// ChartObject::Draw側で判定円ループの中で計算済みの値を渡してもらう（6レーン分）
	void Draw(Camera* cam, const DirectX::SimpleMath::Matrix& view,
		int combo, int perfect, int great, int good, int miss,
		const DirectX::SimpleMath::Vector3 judgeCircleScreenPos[6],
		const DirectX::SimpleMath::Vector3 judgeCirclePos3D[6],
		const DirectX::SimpleMath::Vector3 laneColors[6]);

private:

	Texture2D m_ComboDigits[4];	// コンボ数の3桁表示
	Texture2D m_PerfectDigits[4];
	Texture2D m_GreatDigits[4];
	Texture2D m_GoodDigits[4];
	Texture2D m_MissDigits[4];

	Texture2D m_BreakdownLabelPerfect;
	Texture2D m_BreakdownLabelGreat;
	Texture2D m_BreakdownLabelGood;
	Texture2D m_BreakdownLabelMiss;

	Texture2D m_JudgeRankPerfect;
	Texture2D m_JudgeRankGreat;
	Texture2D m_JudgeRankGood;
	JudgeRankEffect m_JudgeRankEffects[6];
	static constexpr float kJudgeRankPopDuration = 0.2f; // 表示継続時間（秒）

	static constexpr float kJudgeRankMaxAngle = 12.0f;	//傾きの最大度
	std::mt19937 m_Rng{ std::random_device{}() };

	BarrierEffect m_BarrierEffects[6];
	BarrierQuad m_BarrierQuad;
	static constexpr float kBarrierDuration = 10.25f;

	float m_ComboPopTimer = 0.0f;		// コンボ数字をポップさせる残り時間
	float m_NuberSplite_Size = 100.0f;	// コンボ数字1桁の表示サイズ

	Texture2D m_FullComboTex;	// 「FULL COMBO」バナー
	bool m_FullComboActive = false;	// 曲終了〜シーン切り替えまで表示し続ける
};