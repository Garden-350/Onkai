#pragma once
#include "Texture2D.h"
#include "BarrierQuad.h"
#include <SimpleMath.h>

class Camera;

struct BarrierEffect
{
	bool active = false;
	float timer = 0.0f;
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

	void Update(float deltaTime);

	// judgeCircleScreenPos/judgeCirclePos3D/laneColors はいずれも
	// ChartObject::Draw側で判定円ループの中で計算済みの値を渡してもらう（6レーン分）
	void Draw(Camera* cam, const DirectX::SimpleMath::Matrix& view,
		int combo, int perfect, int great, int good, int miss,
		const DirectX::SimpleMath::Vector3 judgeCircleScreenPos[6],
		const DirectX::SimpleMath::Vector3 judgeCirclePos3D[6],
		const DirectX::SimpleMath::Vector3 laneColors[6]);

private:
	struct JudgeRankEffect
	{
		bool active = false;
		float timer = 0.0f;
		int rank = 0; // 0=PERFECT, 1=GREAT, 2=GOOD
	};

	Texture2D m_ComboDigits[4];	// コンボ数の3桁表示
	Texture2D m_PerfectDigits[4];
	Texture2D m_GreatDigits[4];
	Texture2D m_GoodDigits[4];
	Texture2D m_MissDigits[4];

	Texture2D m_JudgeRankPerfect;
	Texture2D m_JudgeRankGreat;
	Texture2D m_JudgeRankGood;
	JudgeRankEffect m_JudgeRankEffects[6];
	static constexpr float kJudgeRankPopDuration = 0.6f; // 表示継続時間（秒）

	BarrierEffect m_BarrierEffects[6];
	BarrierQuad m_BarrierQuad;
	static constexpr float kBarrierDuration = 10.25f;

	float m_ComboPopTimer = 0.0f;		// コンボ数字が拡大表示される残り時間
	float m_NuberSplite_Size = 80.0f;	// コンボ数字1桁の表示サイズ
};