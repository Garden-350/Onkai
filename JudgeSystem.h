#pragma once
#include <vector>
#include <functional>

struct ChartNote;
class Sound;

// ============================================================
// JudgeSystem : TAP/FLICK/LONGNOTEの判定ロジックと、判定カウント
//   (Perfect/Great/Good/Miss/Combo/MaxCombo)を担当するクラス。
//   パーティクル・ライトエフェクト・判定ランク演出などの「見た目」は
//   ここでは一切扱わず、コールバック経由でChartObject側に委譲する。
// ============================================================
class JudgeSystem
{
public:
	// ヒット成立時に呼ばれる (laneIndex, rank: 0=PERFECT,1=GREAT,2=GOOD)
	using OnJudgeFn = std::function<void(int laneIndex, int rank)>;
	// TAPノーツが無いレーンを空押ししたときに呼ばれる
	using OnEmptyPressFn = std::function<void(int laneIndex)>;
	using OnLongEndFn = std::function<void(int laneIndex)>;
	//チャージノーツを取った時に呼ぶ（スペース押したときに発動）
	using OnChargeHitFn = std::function<void(int rank)>;
	void SetOnChargeHit(OnChargeHitFn fn) { m_OnChargeHit = fn; }

	void SetOnLongEnd(OnLongEndFn fn) { m_OnLongEnd = fn; }

	void SetOnJudge(OnJudgeFn fn) { m_OnJudge = fn; }
	void SetOnEmptyPress(OnEmptyPressFn fn) { m_OnEmptyPress = fn; }

	// TAPノーツ：キーを押した瞬間の判定
	void JudgeLane(int key, int laneIndex, std::vector<ChartNote>& notes, float musicTime, Sound& sound);
	// FLICKノーツ：時間ウィンドウ内でサイド一致を判定
	void JudgeFlickNotes(std::vector<ChartNote>& notes, float musicTime, int playerSide, Sound sound);
	// LONGNOTE：押し始め〜押しっぱなし
	void JudgeLongNoteHold(std::vector<ChartNote>& notes, float musicTime);
	// オートモード用：到達時刻を過ぎたらパーフェクトに確定させる
	void UpdateAutoJudgge(std::vector<ChartNote>& notes, float musicTime, Sound& sound);
	// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
	void MarkTapAutoMiss(std::vector<ChartNote>& notes, float musicTime);
	//FLICKノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
	void MarkFlickAutoMiss(std::vector<ChartNote>& notes, float musicTime);
	// LoadChart時に呼ぶ：Perfect/Great/Good/Missのみリセット（Combo/MaxComboは元の仕様通りリセットしない）
	void ResetJudgeCounts();
	//チャージ中はロングを離してもミスにしない時間帯
	void SetMissFressWindow(float start, float end) { m_MissFreeStart = start; m_MissFreeEnd = end; }

	int GetPerfectCount() const { return m_Perfect; }
	int GetGreatCount()   const { return m_Great; }
	int GetGoodCount()    const { return m_Good; }
	int GetMissCount()    const { return m_Miss; }
	int GetMaxComboCount() const { return m_MaxCombo; }
	int GetComboCount()   const { return m_Combo; } // Draw()でのコンボ数字表示用
	static constexpr float kLongTickInterval = 0.2f;	// ロングノーツの継続判定(tick)間隔
	static constexpr float kTurnGrace = 0.12f;

private:
	void RegisterHit(int laneIndex, int rank); // m_Combo++ / m_MaxCombo更新 + m_OnJudgeコールバック呼び出し
	void RegisterMiss();	//コンボリセット
	void NotifyCharge(const ChartNote& n, int rank);

	int m_Perfect = 0;
	int m_Great = 0;
	int m_Good = 0;
	int m_Miss = 0;
	int m_Combo = 0;
	int m_MaxCombo = 0;

	float m_MissFreeStart = -1.0f;
	float m_MissFreeEnd = -1.0f;

	OnJudgeFn m_OnJudge;
	OnEmptyPressFn m_OnEmptyPress;
	OnLongEndFn m_OnLongEnd;
	OnChargeHitFn m_OnChargeHit;

	//判定タイミングの許容誤差（秒）。値が小さいほどシビア
	static constexpr float kPerfect = 0.06f;
	static constexpr float kGreat = 0.08f;
	static constexpr float kGud = 0.1f;			// この範囲を過ぎたら自動ミス
	//フリックの判定幅
	static constexpr float kFlickPerfect = 0.08f;
	static constexpr float kFlickGreat = 0.12f;
	static constexpr float kFlickGood = 0.15f;	//これ以上遅れたらミス
};