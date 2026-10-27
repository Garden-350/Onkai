#pragma once
#include <vector>
#include <random>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"
#include "Texture2D.h"

class Camera;

// ============================================================
// WeakPoint : 敵の弱点
//   ・弱点は「どのキーか（S D F J K L）」と「敵の体のどこか」の2つを持つ
//     kBarsPerSwitch 小節ごとに、キーも場所もランダムで変わる
//   ・弱点の場所に、そのキーの画像（S.png など）を出す。まわりの輪が残り時間（真上から時計回りに減る）
//   ・普段の攻撃は、どのレーンを叩いても弱点の場所へ飛ぶ（弱点のキーで叩くと強い攻撃になる）
//   ・プレイヤーは判定円を見ているので、弱点のレーンの判定円も六角形で囲む
//   ・光り方は曲の拍に合わせて脈打つ
//
//   必殺技の流れ
//     発動（BeginSpecial）… 弱点を選び直し、撃つまで固定
//     レーンのキーを押す（SetAim）… 狙いがそのレーンへ動く（判定円に枠が出る）
//     Space（Lock）… 狙いを固定する（以後キーを押しても動かない）
//     発射（OnFired）… ビームは敵のど真ん中へ。狙いが弱点のキーと合っていれば大ダメージ
//     発射が終わった（EndSpecial）… 普段の切り替えに戻る
// ============================================================
class WeakPoint
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;

	void Init();
	void SetLaneColor(int lane, const Vector3& color);	// レーンの色（ChartObject のレーン色を渡す）
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
	int  GetWeakLane() const { return m_WeakLane; }	// 弱点のキー（まだ無い時は -1）
	Vector3 GetWeakPos() const;						// 弱点の場所（ワールド座標。まだ無い時は敵の中心）

	// ---- 調整用：切り替え ----
	static constexpr int   kBarsPerSwitch = 4;		// 何小節ごとに弱点が変わるか

	// ---- 調整用：弱点が出る場所（敵の中心のまわり。カメラから見た面の上）----
	static constexpr float kPosRadiusMin = 30.0f;	// 敵の中心から、少なくともこれだけ離す
	static constexpr float kPosRadiusMax = 150.0f;	// 敵の中心から、最大でこれだけ離す
	static constexpr float kPosMinMove = 90.0f;		// 前の場所から、少なくともこれだけ動かす
	static constexpr float kPosLift = 30.0f;		// 出る範囲の中心を、敵の中心から画面の上へずらす
	static constexpr float kPosFront = 120.0f;		// カメラの方へどれだけ出すか（敵の体に埋もれないように）

	// ---- 調整用：見た目（敵の上の弱点）----
	static constexpr float kKeyIconSize = 72.0f;	// キーの画像の大きさ（画面のピクセル）
	static constexpr float kKeyIconPop = 0.5f;		// 出た瞬間にどれだけ大きく出るか（0.5 なら 1.5倍から縮む）
	static constexpr float kWeakRadius = 46.0f;		// キーの画像の後ろの六角形
	static constexpr float kTimerRadius = 62.0f;	// 残り時間の輪
	static constexpr float kBeatKick = 0.15f;		// 拍ごとにどれだけ膨らむか
	static constexpr float kWarnRatio = 0.25f;		// 残りがこの割合を切ったら、輪が赤くなって半拍ごとに脈打つ
	static constexpr float kAimRadius = 80.0f;		// 狙いの枠（ロック前）
	static constexpr float kLockRadius = 64.0f;		// 狙いの枠（ロック後）
	static constexpr bool  kShowMatchHint = true;	// 狙いが弱点のキーと合っていたら、枠を金色にして敵の方にも出す

	// ---- 調整用：判定円の印 ----
	static constexpr bool  kShowLaneMarker = true;		// 弱点のレーンの判定円に印を出すか
	static constexpr float kLaneMarkerRadius = 72.0f;	// 判定円を囲む六角形の大きさ
	static constexpr float kLaneMarkerAlpha = 0.85f;	// 判定円の印の濃さ

private:
	enum class Mode { Normal, Aiming, Fired };

	void PickNew();		// 弱点のキーと場所を、ランダムで選び直す
	void Basis(Vector3& right, Vector3& up, Vector3& toCam) const;	// 敵から見たカメラ向きの面
	float BeatPulse(float beatsPerPulse, float sharpness) const;	// 拍の頭で 1、だんだん 0 に戻る
	float Remain() const;	// 弱点でいられる残りの割合（1 → 0）
	void Flush(Camera* cam, float width, float intensity, const Vector3& glow);	// たまった点を描いて空にする
	void DrawTimerRing(Camera* cam, const Vector3& center, const Vector3& right, const Vector3& up,
		float radius, float width, const Vector3& laneColor, float alpha);
	void DrawBrackets(Camera* cam, const Vector3& center, float radius, float spin, const Vector3& color, bool locked);
	static Vector3 ToScreen(Camera* cam, const Vector3& world);	// ワールド座標 → Texture2D の座標（画面の真ん中が 0,0）

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	Texture2D m_KeyIcon[6];	// S D F J K L の画像
	Vector3 m_LaneColor[6];
	Vector3 m_Anchor[6];
	bool  m_HasAnchor[6] = {};

	Vector3 m_Center;
	Vector3 m_CamPos;
	bool  m_HasCenter = false;
	bool  m_HasCam = false;
	float m_OffX = 0.0f, m_OffY = 0.0f;	// 弱点の場所（カメラから見た面の上での、横と縦のずれ）

	Mode  m_Mode = Mode::Normal;
	int   m_WeakLane = -1;
	float m_PhaseStart = 0.0f;	// 今の弱点になった時刻
	float m_PhaseEnd = 0.0f;	// 次に変わる時刻（必殺技中は撃つ時刻）
	float m_Interval = 0.0f;	// kBarsPerSwitch 小節の秒数
	float m_BeatLen = 0.5f;		// 1拍の秒数
	float m_Now = 0.0f;			// 曲の時刻
	float m_Time = 0.0f;		// 見た目の動き用の時間

	int   m_Aim = 2;
	bool  m_Locked = false;
	float m_LockFlash = 0.0f;	// ロックした瞬間 1 → 0
	float m_SwitchFlash = 0.0f;	// 弱点が変わった瞬間 1 → 0

	std::mt19937 m_Rng{ 2468u };
};
