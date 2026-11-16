#pragma once
#include "Object.h"
#include "LongBeamRibbon.h"
#include <vector>
#include <random>
#include <chrono>

class JudgeSquareFx : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;
	Camera::Layer GetLayer() const override { return Camera::Layer::Playfield; }

	//判定ごとの色　四角と星の色担当
	static DirectX::SimpleMath::Vector3 RankColor(int rank)
	{
		switch (rank)
		{
		case 0:	//WEAK 赤
			return DirectX::SimpleMath::Vector3(1.0f, 0.18f, 0.18f);
		case 1:	//HIT　青
			return DirectX::SimpleMath::Vector3(0.2f, 0.45f, 1.0f);
		default:	//GRAZE　黄色
			return DirectX::SimpleMath::Vector3(1.0f, 0.85f, 0.15f);
		}
	}

private:
	using Vector3 = DirectX::SimpleMath::Vector3;

	struct Square
	{
		Vector3 pos;
		Vector3 color;
		float scale = 1.0f;	//判定で大きさを変えるよう
		float age = 0.0f;
	};
	struct Star
	{
		Vector3 pos, vel;
		Vector3 color;
		float size = 20.0f;
		float rot = 0.0f;	//傾き
		float spin = 0.0f;	//廻る速さ
		float age = 0.0f;
	};

	void SpawnSquare(const Vector3& pos, int rank);
	void SpawnStar(const Vector3& pos, const Vector3& color);
	//a~bへ、同じ濃さの線
	void DrawLine(Camera* cam, const Vector3& a, const Vector3& b, const Vector3& color, float alpha, float width, float intensity);
	//真ん中が濃くて両端が消える線（星の光の筋）
	void DrawStreak(Camera* cam, const Vector3& center, const Vector3& dir, float len, const Vector3& color, float alpha, float width, float intensity);
	void DrawSquare(Camera* cam, const Vector3& center, float size, const Vector3& color, float alpha, float width);
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	std::vector<Square> m_Squares;
	std::vector<Star> m_Stars;
	std::mt19937 m_Rng{ 2468u };

	int m_JudgeSub = 0;
	float m_Time = 0.0f;
	float m_LastLongTick[6] = 
	{ -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };	//レーンごとに最後にロングの判定が来た時刻
	int m_LongTickCount[6] = { 0, 0, 0, 0, 0, 0 };
	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;

	//調整用
	static constexpr float kSquareTime = 0.35f;	//四角が消えるまでの秒数
	static constexpr float kSquareStart = 70.0f;	//出た瞬間の大きさ
	static constexpr float kSquareEnd = 170.0f;		//消えるときの大きさ
	static constexpr float kSquareLine = 30.0f;		//線の太さ
	static constexpr float kLongGap = 0.3f;			//これより間が空いたロング判定は押し始めとみなす
	static constexpr float kStarTime = 0.6f;		//星が消えるまでの秒数
	static constexpr float kStarSize = 20.0f;
	static constexpr float kStarRise = 160.0f;		//星が上へ昇る速さ

	static constexpr int   kStarEvery = 2;			//ロングの判定何回ごとに
	static constexpr int   kMaxSquares = 24;
	static constexpr int   kMaxStars = 48;
};

