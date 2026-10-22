#pragma once
#include "Texture2D.h"
#include <random>

// ============================================================
// ChargeAura : 必殺技のチャージ小節＝フィーバータイムの背景演出（背景を金色にして盛り上げる）
//   重ねる順（奥 → 手前）
//     1. 金の下地      … レーンの後ろ（中央）は暗い琥珀、外側ほど明るい金
//     2. 回る光の帯    … 放射状の帯がぐるぐる回る（拍の瞬間だけ速く回る）
//     3. 拍の明滅      … 拍の瞬間に全体が少し明るくなる
//     4. 集中線        … 3パターンを数フレームごとに切り替えてチラつかせる（最後の1拍は毎フレーム）
//     5. 光の輪        … 拍ごとに中心から広がる
//     6. 火の粉        … 下から上へ。チャージが進むほど数も速さも増える
//     7. きらめき      … レーンの外にキラッと出ては消える
//     8. 縁の光        … 画面の縁が光る
//     9. フラッシュ    … 開始の瞬間（弱）と発射の瞬間（強）
//   使う画像（assets/UI/charge/）：
//     charge_bg / charge_burst / charge_edge / charge_lines_sheet / charge_ring / charge_spark / charge_star / charge_flash（すべて .png）
//
//   使い方
//     ChargeAura::Instance().Start();                 チャージ小節が始まった瞬間（拍にそろえた後）
//     ChargeAura::Instance().Update(dt, progress);     毎フレーム。progress = チャージ小節の進み 0～1
//     ChargeAura::Instance().Release();               小節が終わってビームを撃つ瞬間
//     ChargeAura::Instance().Draw(cam);               背景の直後（敵・レーンより先）に描く
// ============================================================
class ChargeAura
{
public:
	static ChargeAura& Instance() { static ChargeAura s; return s; }

	void Init();
	void Start();
	void Release();
	void Cancel();						// 演出を即消す（ポーズからやり直し・曲終了など）
	void Update(float dt, float progress);
	void Draw(Camera* cam);

	bool IsVisible() const { return m_State != State::Idle || m_Flash > 0.0f; }
	float GetIntensity() const { return m_Intensity; }	// 0～1。レーンの下地を濃くする等に使える

private:
	enum class State { Idle, Charging, Releasing };

	// ---- 見た目の調整はここ ----
	static constexpr int   kBeatsPerBar = 4;
	static constexpr float kFadeIn = 0.15f;		// 金になるまでの秒数
	static constexpr float kFadeOut = 0.35f;	// 発射後に消えるまでの秒数
	static constexpr float kBgAlpha = 0.92f;	// 下地の濃さ（1で元の背景が完全に隠れる）
	static constexpr float kFocusX = 0.0f;		// 集中線・光の輪の中心（画面座標）
	static constexpr float kFocusY = 70.0f;		// レーンの消失点あたり
	static constexpr float kLinesSize = 1700.0f;	// 集中線の大きさ（回しても角が出ないよう画面より大きく）
	static constexpr float kLineSwap = 0.05f;		// 集中線を切り替える間隔
	static constexpr float kLineSwapLast = 1.0f / 60.0f;	// 最後の1拍の切り替え間隔
	static constexpr float kRingLife = 0.45f;
	static constexpr float kRingFrom = 180.0f;
	static constexpr float kRingTo = 1400.0f;
	static constexpr int   kMaxRings = 4;
	static constexpr int   kMaxSparks = 64;
	static constexpr float kBurstSpin = 30.0f;		// 光の帯の回転（度/秒）
	static constexpr float kBurstSize = 2000.0f;
	static constexpr float kBurstAlpha = 0.35f;
	static constexpr float kBeatFlash = 0.14f;		// 拍の明滅の強さ
	static constexpr int   kMaxStars = 24;
	// きらめきを出さない範囲（レーンの上）
	static constexpr float kStarAvoidHalfW = 360.0f;
	static constexpr float kStarAvoidTopY = 120.0f;

	struct Ring { float age = 0.0f; bool alive = false; };
	struct Spark { float x = 0, y = 0, vy = 0, size = 0, phase = 0; bool alive = false; };
	struct Star { float x = 0, y = 0, age = 0, life = 0, size = 0, rot = 0; bool alive = false; };

	float Rand(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(m_Rng); }
	static void DrawQuad(Texture2D& t, Camera* cam, float x, float y, float w, float h, float rotDeg, float alpha);

	Texture2D m_Bg, m_Burst, m_Lines, m_Ring, m_Spark, m_Star, m_Edge, m_FlashTex;

	State m_State = State::Idle;
	float m_Intensity = 0.0f;	// 全体の濃さ（フェード）
	float m_Progress = 0.0f;
	int   m_LastBeat = -1;
	float m_Pulse = 0.0f;		// 拍の瞬間 1 → すぐ減る
	float m_Flash = 0.0f;
	int   m_LineFrame = 0;
	float m_LineRot = 0.0f;
	float m_LineTimer = 0.0f;
	float m_SparkAcc = 0.0f;
	float m_StarAcc = 0.0f;
	float m_BurstRot = 0.0f;
	Ring  m_Rings[kMaxRings];
	Spark m_Sparks[kMaxSparks];
	Star  m_Stars[kMaxStars];
	std::mt19937 m_Rng{ 12345 };
};
