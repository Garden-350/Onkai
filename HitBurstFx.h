#pragma once
#include "Object.h"
#include "LongBeamRibbon.h"
#include <vector>
#include <random>
#include <chrono>

class HitBurstFx : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;
	Camera::Layer GetLayer() const override { return Camera::Layer::Playfield; }

	//レーザーが向かう場所
	void SetTarget(const DirectX::SimpleMath::Vector3& pos) { m_Target = pos; }

	//判定ごとの色。ここを変えれば、破片とレーザーの色が変わる
	static DirectX::SimpleMath::Vector3 RankColor(int rank)
	{
		switch (rank)
		{
		case 0:  return DirectX::SimpleMath::Vector3(1.0f, 0.82f, 0.30f);	//WEAK：金
		case 1:  return DirectX::SimpleMath::Vector3(0.30f, 0.85f, 1.0f);	//HIT：水色
		default: return DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);	//GRAZE：紫
		}
	}
private:
	using Vector3 = DirectX::SimpleMath::Vector3;

	struct Piece
	{
		//破片一つ
		Vector3 pos, vel;
		float width;
		float spin;	//漂うときの揺れかた（破片ごとに調整）
	};
	struct Burst
	{
		//ノーツ一つ分の破片の塊
		std::vector<Piece> piece;
		Vector3 color;
		int rank = 1;
		float age = 0.0f;	//弾けてからの秒数
	};
	struct Laser	//破片1つ分の攻撃：まず発射位置へ飛び、そこから敵へまっすぐ撃つ
	{
		Vector3 from;	//破片がいた所
		Vector3 launch;	//レーザーを撃つ所（敵のまわり）
		Vector3 to;		//当たる所
		Vector3 color;
		int rank = 1;
		float width = 6.0f;
		float flareSize = 3.0f;	//先の光の玉の大きさ
		float flareRot = 0.0f;		//光の筋の傾き
		float age = 0.0f;			//マイナスの間は、まだ撃つ前（ずらし）
		bool arrived = false;
		bool notify = false;		//届いた時に「当たった」を知らせるか（かたまりに1本だけ）
	};

	void SpawnBurst(const Vector3& pos, int rank);
	void FireReady();			//待っている塊を撃つ（拍の頭）
	void FireBurst(Burst& b);	//塊の破片をすべてレーザー化
	void LaunchLaser(const Vector3& from, const Vector3& color, int rank, bool notify, float delay);
	void DrawFlare(Camera* cam, const Vector3& pos, const Vector3& color, float size, float rot, float alpha);

	//Vector3 Bezier(const Laser& l, float s) const;
	static float Ease(float x) { return x * (0.55f + 0.45f * x); }	//だんだん速くなる
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	std::vector<Burst> m_Bursts;
	std::vector<Laser> m_Lasers;
	std::mt19937 m_Rng{ 1357u };
	Vector3 m_Target = Vector3(0.0f, 80.0f, 200.0f);

	int m_JudgeSub = 0;
	int m_BeatSub = 0;
	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;

	//調整用
	static constexpr float kLift = 30.0f;	//判定円からどれだけ上で弾けるか
	static constexpr float kMinHover = 0.1f;	//弾けてからこれだけ待ってから撃つ
	static constexpr float kMaxWait = 1.0f;		//拍が来なくてもこの時間待ったら発射
	static constexpr int   kFireEveryBeats = 1;		//何拍ごとに撃つか（2なら2拍に1回）
	static constexpr float kFireSpread = 0.4f;		//破片ごとに撃つ時間をずらす最大の秒数（一斉射撃に見せる）
	static constexpr float kHitFlashTime = 0.10f;	//届いた時に光の玉が広がって消えるまでの秒数
	static constexpr float kDrag = 9.0f;			//破片の減速（大きいほど早く止まる）
	static constexpr float kDashTime = 0.12f;		//破片が発射位置へ飛ぶ秒数
	static constexpr float kLaserTime = 0.18f;		//発射位置から敵に届くまでの秒数
	static constexpr float kLaserTail = 0.6f;		//レーザーの尾の長さ（発射位置から敵までの割合）
	static constexpr float kLaunchDistMin = 600.0f;	//発射位置の、敵からの距離
	static constexpr float kLaunchDistMax = 900.0f;
	static constexpr int   kMaxBursts = 4;
	static constexpr int   kMaxLasers = 10;
};

