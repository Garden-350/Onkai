#pragma once
#include <vector>
#include <random>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;

// ============================================================
// WeakPoint : 敵の弱点部位
//   ・敵のまわりに、S D F J K L に対応する6つの部位が「冠」のような弧に並ぶ
//     （画面の左から順に S D F J K L。カメラの向きに合わせて並べるので、つぶれて一直線にならない）
//   ・そのうち1つが「弱点」。kBarsPerSwitch 小節ごとにランダムで別の部位へ移る
//   ・弱点のまわりの輪が、弱点でいられる残り時間を表す（真上から時計回りに減っていく）
//   ・プレイヤーは判定円を見ているので、弱点のレーンの判定円にも六角形の印を出す
//   ・光り方は曲の拍に合わせて脈打つ（バラバラにチカチカしない）
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
	void SetLaneAnchor(int lane, const Vector3& pos);	// 判定円の位置（毎フレーム渡す）
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

	// ---- 調整用：6つの部位の並び（敵のまわりの「冠」の形）----
	//   画面の上から見て、真上を 0 度として、左右に ±kHaloSpanDeg の範囲に6つ並べる
	static constexpr float kHaloRadius = 190.0f;	// 敵の中心からの距離（冠の大きさ）
	static constexpr float kHaloSpanDeg = 105.0f;	// 一番外側（S と L）の角度。90 で真横、大きいほど下まで回り込む
	static constexpr float kHaloLift = 30.0f;		// 冠の中心を、敵の中心から画面の上へどれだけずらすか
	static constexpr float kHaloFront = 120.0f;		// カメラの方へどれだけ出すか（敵の体に埋もれないように）

	// ---- 調整用：見た目（敵のまわり）----
	static constexpr float kMarkRadius = 12.0f;		// 弱点でない部位の小さい印
	static constexpr float kMarkAlpha = 0.22f;		// 弱点でない部位の印の濃さ（0 で消える）
	static constexpr float kWeakRadius = 42.0f;		// 弱点の六角形
	static constexpr float kTimerRadius = 60.0f;	// 残り時間の輪
	static constexpr float kBeatKick = 0.18f;		// 拍ごとに弱点がどれだけ膨らむか
	static constexpr float kWarnRatio = 0.25f;		// 残りがこの割合を切ったら、輪が赤くなって半拍ごとに脈打つ
	static constexpr float kAimRadius = 80.0f;		// 狙いの枠（ロック前）
	static constexpr float kLockRadius = 58.0f;		// 狙いの枠（ロック後）
	static constexpr bool  kShowMatchHint = true;	// 狙いが弱点と合っていたら、枠を金色にする

	// ---- 調整用：判定円の印 ----
	static constexpr bool  kShowLaneMarker = true;		// 弱点のレーンの判定円に印を出すか
	static constexpr float kLaneMarkerRadius = 72.0f;	// 判定円を囲む六角形の大きさ
	static constexpr float kLaneMarkerAlpha = 0.85f;	// 判定円の印の濃さ

private:
	enum class Mode { Normal, Aiming, Fired };

	void PickNewLane();	// 今と違う部位をランダムで選ぶ
	void Basis(Vector3& right, Vector3& up, Vector3& toCam) const;	// 敵から見たカメラ向きの面
	float BeatPulse(float beatsPerPulse, float sharpness) const;	// 拍の頭で 1、だんだん 0 に戻る
	float Remain() const;	// 弱点でいられる残りの割合（1 → 0）
	void Flush(Camera* cam, float width, float intensity, const Vector3& glow);	// たまった点を描いて空にする
	void DrawTimerRing(Camera* cam, const Vector3& center, const Vector3& right, const Vector3& up,
		float radius, float width, const Vector3& laneColor, float alpha);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	Vector3 m_LaneColor[6];
	Vector3 m_Anchor[6];
	bool  m_HasAnchor[6] = {};
	bool  m_Flip = false;	// 判定円が画面の右から S D F … と並んでいる時 true（冠の並びも合わせる）

	Vector3 m_Center;
	Vector3 m_CamPos;
	bool  m_HasCenter = false;
	bool  m_HasCam = false;

	Mode  m_Mode = Mode::Normal;
	int   m_WeakLane = -1;
	float m_PhaseStart = 0.0f;	// 今の弱点になった時刻
	float m_PhaseEnd = 0.0f;	// 次に移る時刻（必殺技中は撃つ時刻）
	float m_Interval = 0.0f;	// kBarsPerSwitch 小節の秒数
	float m_BeatLen = 0.5f;		// 1拍の秒数
	float m_Now = 0.0f;			// 曲の時刻
	float m_Time = 0.0f;		// 見た目の動き用の時間

	int   m_Aim = 2;
	bool  m_Locked = false;
	float m_LockFlash = 0.0f;	// ロックした瞬間 1 → 0
	float m_SwitchFlash = 0.0f;	// 弱点が移った瞬間 1 → 0

	std::mt19937 m_Rng{ 2468u };
};
