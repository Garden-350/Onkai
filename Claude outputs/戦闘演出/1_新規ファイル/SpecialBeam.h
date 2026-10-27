#pragma once
#include <vector>
#include <random>
#include <functional>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;

// ============================================================
// SpecialBeam : 必殺技の発射ビーム（見た目だけ）
//   3つの層でできている
//   ・本命の帯 … 太い金色の帯が、弧を描いて狙った部位へ伸びる。当たった瞬間に OnHit
//   ・群れ     … 細い色とりどりの光が、ランダムな方向へふくらんでから同じ点に集まる
//                （1本ごとに向き・高さ・速さ・色・遅れが違う）。1本当たるたびに OnSubHit
//   ・雷       … 本命の帯にまとわりつく、ギザギザの青白い光。細かく形が変わり続ける
//   チャージ量（0～1）が多いほど、太く明るくなる
// ============================================================
class SpecialBeam
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;
	// 本命の帯が当たった瞬間（1回だけ）。critical : 弱点に当たったか
	using OnHitFn = std::function<void(float chargeRatio, bool critical, const Vector3& hitPos)>;
	// 群れの光が1本当たるたび（小さな火花用）
	using OnSubHitFn = std::function<void(const Vector3& hitPos, const Vector3& color)>;
	void SetOnHit(OnHitFn fn) { m_OnHit = fn; }
	void SetOnSubHit(OnSubHitFn fn) { m_OnSubHit = fn; }

	void Init();
	// from : 撃つ所（プレイヤー）、to : 当てる所（狙った部位）、chargeRatio : 0～1、critical : 弱点か
	void Fire(const Vector3& from, const Vector3& to, float chargeRatio, bool critical);
	void Update(float deltaTime);
	void Draw(Camera* cam);
	void Cancel() { m_Active = false; }	// リトライの時など、途中で止める

	bool IsActive() const { return m_Active; }
	// 撃ってから全部消えるまでの秒数（SpecialMove::kFireDuration はこれ以上にする）
	static float TotalDuration()
	{
		float mainEnd = kMainGrow + kMainHold + kMainFade;
		float swarmEnd = kSwarmDelayMax + kSwarmGrowMax + kSwarmTailTime;
		return (mainEnd > swarmEnd) ? mainEnd : swarmEnd;
	}

	// ---- 調整用：本命の帯 ----
	static constexpr float kMainGrow = 0.22f;		// 先が当たるまでの秒数
	static constexpr float kMainHold = 0.30f;		// つながったままの秒数
	static constexpr float kMainFade = 0.30f;		// 消えるまでの秒数
	static constexpr float kMainArc = 380.0f;		// 弧の高さ
	static constexpr float kMainWidthMin = 50.0f;	// チャージ0の時の太さ
	static constexpr float kMainWidthMax = 140.0f;	// チャージ満タンの時の太さ
	static constexpr int   kMainSegments = 64;

	// ---- 調整用：群れ ----
	static constexpr int   kSwarmCount = 24;		// 本数（増やしても、向きはばらける）
	static constexpr float kSwarmDelayMax = 0.18f;	// 撃ち出しの遅れ（0～この秒数でばらばら）
	static constexpr float kSwarmGrowMin = 0.25f;	// 当たるまでの秒数（最短）
	static constexpr float kSwarmGrowMax = 0.45f;	// 当たるまでの秒数（最長）
	static constexpr float kSwarmTailTime = 0.20f;	// 当たってから尾が消えるまでの秒数
	static constexpr float kSwarmTrail = 0.35f;		// 尾の長さ（道のりの割合）
	static constexpr float kSwarmSpreadDeg = 100.0f;// 広がる向きの範囲（0 = 真上、±この角度まで横へ）
	static constexpr float kSwarmRadiusMin = 250.0f;// どれだけふくらむか（最小）
	static constexpr float kSwarmRadiusMax = 650.0f;// どれだけふくらむか（最大）
	static constexpr float kSwarmWidthMin = 10.0f;
	static constexpr float kSwarmWidthMax = 22.0f;
	static constexpr int   kSwarmSegments = 40;

	// ---- 調整用：雷 ----
	static constexpr int   kBolts = 3;				// 本数
	static constexpr int   kBoltSegments = 28;		// ギザギザの細かさ
	static constexpr float kBoltJitter = 45.0f;		// 帯からどれだけ外れるか
	static constexpr float kBoltRefresh = 0.045f;	// 形を作り直す間隔（秒）
	static constexpr float kBoltWidthMin = 6.0f;
	static constexpr float kBoltWidthMax = 14.0f;

private:
	struct Strand
	{
		Vector3 c1, c2;		// 3次ベジェ曲線の真ん中の2点
		float delay;		// 撃ち出しの遅れ
		float grow;			// 当たるまでの秒数
		float width;
		Vector3 color;
		bool arrived;
	};

	Vector3 MainPoint(float s) const;	// 本命の帯の弧の上の点（s : 0 = 撃つ所、1 = 当てる所）
	static Vector3 Cubic(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, float s);
	void MainHeadTail(float t, float& head, float& tail, float& fade) const;
	void RefreshBolts();
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Points;
	std::vector<Strand> m_Strands;
	std::vector<Vector3> m_BoltOffset;	// 雷のギザギザ（x : 上下方向、y : 左右方向。-1～1）
	float m_BoltWidth[kBolts] = {};
	float m_BoltTimer = 0.0f;

	Vector3 m_From, m_To;
	Vector3 m_AxisU, m_AxisR;	// 進む向きに対して「上」と「横」
	float m_Ratio = 0.0f;
	float m_Time = 0.0f;
	bool  m_Active = false;
	bool  m_HitSent = false;
	bool  m_Critical = false;

	std::mt19937 m_Rng{ 1357u };
	OnHitFn m_OnHit;
	OnSubHitFn m_OnSubHit;
};
