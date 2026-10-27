#pragma once
#include <vector>
#include <random>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;

// ============================================================
// ImpactFx : 着弾の演出（輪・六角形・火花）をまとめて出すクラス
//   ・SpawnSmallHit … 普段の攻撃が当たった（小さい輪と火花）
//   ・SpawnWeakHit  … 普段の攻撃が弱点に当たった（金の輪と六角形と火花）
//   ・SpawnCritical … 必殺技が弱点に当たった（大きい衝撃の輪・白い閃光・たくさんの火花）
//   ・SpawnBlocked  … 防がれた（青白い六角形の盾と、来た方へはね返る火花）
//   どれも LongBeamRibbon で描くので、画像はいらない
// ============================================================
class ImpactFx
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;

	void Init();
	void Update(float deltaTime);
	void Draw(Camera* cam);
	void Clear();	// 全部消す（リトライの時など）

	void SpawnSmallHit(const Vector3& pos, const Vector3& color);
	void SpawnWeakHit(const Vector3& pos, const Vector3& color);
	// power : 0～1（必殺技のチャージ量。多いほど大きく派手になる）
	void SpawnCritical(const Vector3& pos, const Vector3& color, float power);
	// fromDir : 攻撃が飛んできた向き（火花はその逆へはね返る）
	void SpawnBlocked(const Vector3& pos, const Vector3& fromDir);

	// ---- 調整用 ----
	static constexpr int   kMaxRings = 32;		// 同時に出せる輪の数（超えたら古いものから消える）
	static constexpr int   kMaxSparks = 240;	// 同時に出せる火花の数（超えたら古いものから消える）
	static constexpr float kGravity = 900.0f;	// 火花が下に落ちる強さ
	static constexpr float kDrag = 2.5f;		// 火花の減速（大きいほど早く止まる）
	static constexpr float kStreak = 0.035f;	// 火花の尾の長さ（速さ × この秒数）

private:
	struct Ring
	{
		Vector3 pos;
		Vector3 color;
		int   sides;	// 0 = 円、6 = 六角形
		float r0, r1;	// 半径：始め → 終わり
		float width;	// 線の太さ
		float rot;		// 回転（ラジアン）
		float age, life;
	};
	struct Spark
	{
		Vector3 pos, vel;
		Vector3 color;
		float width;
		float age, life;
	};

	void AddRing(const Vector3& pos, const Vector3& color, int sides, float r0, float r1,
		float life, float width, float rot = 0.0f);
	// bias : 火花をかたよらせる向き（Zero ならばらばら）、biasAmount : 0～1
	void AddSparks(const Vector3& pos, const Vector3& color, int count, float speedMin, float speedMax,
		float life, float width, const Vector3& bias, float biasAmount);
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	std::vector<Ring>  m_Rings;
	std::vector<Spark> m_Sparks;
	std::mt19937 m_Rng{ 12345u };
};
