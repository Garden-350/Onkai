#pragma once
#include <vector>
#include <random>
#include <functional>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;

// ============================================================
// AttackOrbs : 普段の攻撃。ノーツを叩くと、判定円から敵の部位へ小さな光が飛ぶ
//   ・左手のレーン（S D F）は左へ、右手のレーン（J K L）は右へ、
//     高く外側にふくらんでから敵へ向かう（レーンの上を低く横切らないように）
//   ・着いた瞬間に OnArrive を呼ぶ（着弾の演出は呼んだ側でやる）
// ============================================================
class AttackOrbs
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;
	using OnArriveFn = std::function<void(int lane, const Vector3& pos, const Vector3& color, bool critical)>;
	void SetOnArrive(OnArriveFn fn) { m_OnArrive = fn; }

	void Init();
	// lane : どのレーンから撃ったか（0～5）、critical : 弱点に飛ぶ光か（太く明るくなる）
	void Launch(int lane, const Vector3& from, const Vector3& to, const Vector3& color, bool critical);
	void Update(float deltaTime);
	void Draw(Camera* cam);
	void Clear() { m_Orbs.clear(); }

	// ---- 調整用 ----
	static constexpr int   kMaxOrbs = 64;			//同時に飛べる数（超えたら古いものから消える）
	static constexpr float kFlightTime = 0.38f;		//飛んでいる秒数
	static constexpr float kArcHeight = 520.0f;		//どれだけ高く上がるか
	static constexpr float kSideOut = 380.0f;		//どれだけ外側へふくらむか
	static constexpr float kTrail = 0.22f;			//尾の長さ（道のりの割合）
	static constexpr float kWidth = 11.0f;			//光の太さ
	static constexpr float kCritWidthMul = 1.7f;	//弱点へ飛ぶ光の太さの倍率
	static constexpr float kIntensity = 0.8f;		//明るさ
	static constexpr float kCritIntensity = 1.8f;	//弱点への明るさ
	static constexpr int   kSegments = 14;			//1つの光の点の数

private:
	struct Orb
	{
		int lane;
		Vector3 p0, p1, p2, p3;	//3次ベジェ曲線の4点（p0 = 判定円、p3 = 敵の部位）
		Vector3 color;
		float t, dur;
		bool crit;
	};
	static Vector3 Bezier(const Orb& o, float s);
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	std::vector<Orb> m_Orbs;
	std::vector<Orb> m_Arrived;	//このフレームで着いたもの（呼び出しはループの外でする）
	std::mt19937 m_Rng{ 777u };
	OnArriveFn m_OnArrive;
};
