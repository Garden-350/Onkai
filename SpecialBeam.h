#pragma once
#include <vector>
#include <random>
#include <functional>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;

class SpecialBeam
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;
	using OnHitFn = std::function<void(float chargeRatio, bool critical, const Vector3& hitPos)>;
	using OnSubHitFn = std::function<void(const Vector3& hitPos, const Vector3& color)>;
	
	void SetOnHit(OnHitFn fn) { m_OnHit = fn; }
	void SetOnSubHit(OnSubHitFn fn) { m_OnSubHit = fn; }

	void Init();
	//from:撃つところ（プレイヤー）　to:当てるところ（敵）,chargeRatio:チャージ量0~1
	void Fire(const Vector3& from, const Vector3& to, float chargeRatio, bool critical);
	void Update(float deltaTime);
	void Draw(Camera* cam);
	void Cancel() { m_Active = false; }	//リトライとかの途中で止める

	bool IsAvtive() const { return m_Active; }
	//撃ってから全部の帯が消えるまで
	static float TotalDuration() 
	{ 
		float mainEnd = kMainGrow + kMainHold + kMainFade;
		float swarmEnd = kSwarmDelayMax + kSwarmGrowMax + kSwarmTailTime;
		return (mainEnd > swarmEnd) ? mainEnd : swarmEnd;
	}

	//調整用 帯
	static constexpr float kMainGrow = 0.22f;		//先が当たるまでの秒数
	static constexpr float kMainHold = 0.30f;		//つながったままの秒数
	static constexpr float kMainFade = 0.30f;		//消えるまでの秒数
	static constexpr float kMainArc = 380.0f;		//弧の高さ
	static constexpr float kMainWidthMin = 50.0f;	//チャージ0の時の太さ
	static constexpr float kMainWidthMax = 140.0f;	//チャージ満タンの時の太さ
	static constexpr int   kMainSegments = 64;

	//調整用　群集
	static constexpr int   kSwarmCount = 24;		//本数（増やしても、向きはばらける）
	static constexpr float kSwarmDelayMax = 0.18f;	//撃ち出しの遅れ（0～この秒数でばらばら）
	static constexpr float kSwarmGrowMin = 0.25f;	//当たるまでの秒数（最短）
	static constexpr float kSwarmGrowMax = 0.45f;	//当たるまでの秒数（最長）
	static constexpr float kSwarmTailTime = 0.20f;	//当たってから尾が消えるまでの秒数
	static constexpr float kSwarmTrail = 0.35f;		//尾の長さ（道のりの割合）
	static constexpr float kSwarmSpreadDeg = 100.0f;//広がる向きの範囲（0 = 真上、±この角度まで横へ）
	static constexpr float kSwarmRadiusMin = 250.0f;//どれだけふくらむか（最小）
	static constexpr float kSwarmRadiusMax = 650.0f;//どれだけふくらむか（最大）
	static constexpr float kSwarmWidthMin = 10.0f;
	static constexpr float kSwarmWidthMax = 22.0f;
	static constexpr int   kSwarmSegments = 40;

	//調整用　エフェクト
	static constexpr int   kBolts = 3;				//本数
	static constexpr int   kBoltSegments = 28;		//ギザギザの細かさ
	static constexpr float kBoltJitter = 45.0f;		//帯からどれだけ外れるか
	static constexpr float kBoltRefresh = 0.045f;	//形を作り直す間隔（秒）
	static constexpr float kBoltWidthMin = 6.0f;
	static constexpr float kBoltWidthMax = 14.0f;
private:
	struct Strand
	{
		Vector3 c1, c2;	//3次ベジェの真ん中の2点
		float delay;	//撃ち出しの遅れ
		float grow;		//当たるまでの秒数
		float width;
		Vector3 color;
		bool arrived;
	};

	Vector3 MainPoint(float s) const;	//帯の弧の上の点(s:0=撃つところ、1=当てることろ）
	static Vector3 Cubic(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, float s);
	void MainHeadTail(float t, float& head, float& tail, float& fade) const;
	void RefreshBolts();
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Points;
	std::vector<Strand> m_Strands;
	std::vector<Vector3> m_BoltOffset;	//雷のエフェクト（x : 上下方向、y : 左右方向。-1～1）
	float m_BoltWidth[kBolts] = {};
	float m_BoltTimer = 0.0f;

	DirectX::SimpleMath::Vector3 m_From;
	DirectX::SimpleMath::Vector3 m_To;
	DirectX::SimpleMath::Vector3 m_AxisU;
	DirectX::SimpleMath::Vector3 m_AxisV;	//進む向きに対して上と横
	float m_Ratio = 0.0f;	//チャージ量
	float m_Time = 0.0f;	//撃ってからの秒数
	bool m_Active = false;
	bool m_HitSent = false;	//OnHitを呼んだか
	bool m_Critical = false;

	std::mt19937 m_Rng{ 1357u };
	OnHitFn m_OnHit;
	OnSubHitFn m_OnSubHit;
};

