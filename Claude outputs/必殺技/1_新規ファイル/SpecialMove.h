#pragma once
#include <vector>
#include <functional>

struct ChartNote;

// ============================================================
// SpecialMove : 必殺技の「状態」と「ゲージ・チャージ量の計算」だけを担当するクラス
//   JudgeSystem と同じ考え方で、見た目（金背景・ビーム・ゲージ表示）や敵へのダメージは扱わない。
//   節目ごとにコールバックを呼ぶので、演出は ChartObject 側で行う。
//
//   状態の流れ
//     Gauge（溜め中）→ 発動 → Waiting（次の拍まで待ち）→ Charging（1小節）→ Firing（発射）→ Gauge
//
//   ChartObject からの呼び方
//     LoadChart の最後      : Reset(m_TotalNote)
//     発動キーを押した時    : TryActivate(...)
//     毎フレーム            : Update(deltaTime, m_MusicTime)
//     判定のたび            : AddHit(rank, isChargeNote)
// ============================================================
class SpecialMove
{
public:
	enum class State { Gauge, Waiting, Charging, Firing };

	using OnChargeStartFn = std::function<void()>;					// チャージ小節が始まった
	using OnFireFn = std::function<void(float chargeRatio)>;		// 小節が終わって撃つ（チャージ量 0～1）
	using OnFinishFn = std::function<void()>;						// 発射が終わって溜め中に戻った
	void SetOnChargeStart(OnChargeStartFn fn) { m_OnChargeStart = fn; }
	void SetOnFire(OnFireFn fn) { m_OnFire = fn; }
	void SetOnFinish(OnFinishFn fn) { m_OnFinish = fn; }

	void Reset(int totalNote);
	// 発動できたら true。次の拍から1小節に入るタップ・フリックに chargeShot の印を付ける
	bool TryActivate(float musicTime, float bpm, int beatsPerBar, std::vector<ChartNote>& notes);
	void Update(float deltaTime, float musicTime);
	// rank : 0=PERFECT 1=GREAT 2=GOOD
	void AddHit(int rank, bool isChargeNote);

	State GetState() const { return m_State; }
	bool  IsReady() const { return m_State == State::Gauge && (m_Gauge >= 1.0f || kDebugAlwaysReady); }
	float GetGauge() const { return m_Gauge; }							// 必殺ゲージ 0～1
	float GetChargeRatio() const;										// チャージ量 0～1
	float GetChargeProgress(float musicTime) const;					// チャージ小節の進み 0～1（金背景用）
	float GetChargeStart() const { return m_ChargeStart; }
	float GetChargeEnd() const { return m_ChargeEnd; }

	// ダメージ = 基本値 × (0.3 + 0.7 × チャージ量)。全部取り逃しても3割は入る
	static float CalcDamage(float baseDamage, float chargeRatio) { return baseDamage * (0.3f + 0.7f * chargeRatio); }

	// ---- 調整用 ----
	static constexpr float kFillRatio = 0.4f;			// 総ノーツの何割を叩いたら満タンか
	static constexpr bool  kDebugAlwaysReady = true;	// 仮：ゲージ無しでいつでも発動（ゲージを作ったら false）
	static constexpr float kFireDuration = 0.6f;		// 発射してから溜め中に戻るまでの秒数（ビームの長さに合わせる）

private:
	void SetState(State s, float musicTime);
	static float RankWeight(int rank);					// P1.0 / G0.6 / Good0.3

	State m_State = State::Gauge;
	int   m_TotalNote = 0;
	float m_Gauge = 0.0f;
	float m_ChargeStart = 0.0f;
	float m_ChargeEnd = 0.0f;
	float m_ChargeWeight = 0.0f;	// 取ったチャージノーツの重みの合計
	int   m_ChargeNoteCount = 0;	// チャージノーツの数
	float m_FireTimer = 0.0f;

	OnChargeStartFn m_OnChargeStart;
	OnFireFn m_OnFire;
	OnFinishFn m_OnFinish;
};
