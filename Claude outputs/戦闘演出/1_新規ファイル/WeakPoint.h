#pragma once
#include <vector>
#include <random>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;

// ============================================================
// WeakPoint : 敵の弱点部位
//   ・敵の体の上に、S D F J K L に対応する6つの部位がある（左から順に並ぶ）
//   ・そのうち1つが「弱点」。kBarsPerSwitch 小節ごとにランダムで別の部位へ移る
//   ・弱点のまわりの輪が、弱点でいられる残り時間を表す（真上から時計回りに減っていく）
//
//   必殺技の流れ
//     発動（BeginSpecial）… 弱点を選び直し、撃つまで固定。狙いの枠が出る
//     レーンのキーを押す（SetAim）… 狙いがそのレーンの部位へ動く
//     Space（Lock）… 狙いを固定する（以後キーを押しても動かない）
//     発射（OnFired）… 狙いの枠を消す
//     発射が終わった（EndSpecial）… 普段の切り替えに戻る
// ============================================================
class WeakPoint
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;

	void Init();
	void SetLaneColor(int lane, const Vector3& color);	// 6つの印の色（ChartObject のレーン色を渡す）
	void Reset();	// 曲の最初に呼ぶ
	// 毎フレーム呼ぶ。enemyCenter : 敵の位置
	void Update(float deltaTime, float musicTime, float bpm, int beatsPerBar, const Vector3& enemyCenter);
	void Draw(Camera* cam);

	// ---- 必殺技 ----
	void BeginSpecial(float musicTime, float fireTime, int startAim);	// fireTime : 撃つ時刻
	void SetAim(int lane);	// 狙いを動かす（チャージ中でロック前の時だけ動く）
	bool Lock();			// 狙いを固定する。固定できたら true
	void OnFired();			// 撃った
	void EndSpecial(float musicTime);	// 発射が終わった

	bool IsAiming() const { return m_Mode == Mode::Aiming; }
	bool IsLocked() const { return m_Locked; }
	int  GetAim() const { return m_Aim; }
	int  GetWeakLane() const { return m_WeakLane; }	// 弱点のレーン（まだ無い時は -1）
	Vector3 GetPoint(int lane) const;				// 部位のワールド座標

	// ---- 調整用：切り替え ----
	static constexpr int   kBarsPerSwitch = 4;		// 何小節ごとに弱点が移るか

	// ---- 調整用：6つの部位の並び（敵の位置から見たずれ）----
	static constexpr float kPointSpacingX = 110.0f;	// 横の間隔
	static constexpr float kPointBaseY = 30.0f;		// 両端の部位の高さ
	static constexpr float kPointArcY = 70.0f;		// 真ん中の部位ほど、どれだけ高くするか
	static constexpr float kPointFrontZ = -120.0f;	// 手前（カメラ側）へどれだけ出すか

	// ---- 調整用：見た目 ----
	static constexpr float kMarkRadius = 16.0f;		// 6つの部位の小さい印
	static constexpr float kMarkAlpha = 0.35f;		// 6つの印の濃さ（0 で消える）
	static constexpr float kWeakRadius = 40.0f;		// 弱点の六角形
	static constexpr float kTimerRadius = 58.0f;	// 残り時間の輪
	static constexpr float kAimRadius = 78.0f;		// 狙いの枠（ロック前）
	static constexpr float kLockRadius = 56.0f;		// 狙いの枠（ロック後）
	static constexpr bool  kShowMatchHint = true;	// 狙いが弱点と合っていたら、枠を金色にする

private:
	enum class Mode { Normal, Aiming, Fired };

	void PickNewLane();	// 今と違う部位をランダムで選ぶ
	void Flush(Camera* cam, float width, float intensity, const Vector3& glow);	// たまった点を描いて空にする

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	Vector3 m_LaneColor[6];

	Vector3 m_Center;
	bool  m_HasCenter = false;

	Mode  m_Mode = Mode::Normal;
	int   m_WeakLane = -1;
	float m_PhaseStart = 0.0f;	// 今の弱点になった時刻
	float m_PhaseEnd = 0.0f;	// 次に移る時刻（必殺技中は撃つ時刻）
	float m_Interval = 0.0f;	// kBarsPerSwitch 小節の秒数
	float m_Now = 0.0f;			// 曲の時刻
	float m_Time = 0.0f;		// 見た目の動き用の時間

	int   m_Aim = 2;
	bool  m_Locked = false;
	float m_LockFlash = 0.0f;	// ロックした瞬間 1 → 0
	float m_SwitchFlash = 0.0f;	// 弱点が移った瞬間 1 → 0

	std::mt19937 m_Rng{ 2468u };
};
