#pragma once
//スコア　＝　満点　＊　（P　＊　重み　＋　G　＊　重み　＋　Good　＊　重み）/　総ノーツ数

namespace ScoreRule
{
	constexpr int kMaxScore = 1000000;	//フルスコア
	constexpr float kPerfect = 1.0f;
	constexpr float kGreat = 0.7f;
	constexpr float kGood = 0.3f;

	inline int Calc(int perfect, int great, int good, int totalNote)
	{
		if (totalNote <= 0) return 0;
		double w = perfect * (double)kPerfect + great * (double)kGreat + good * (double)kGood;
		int score = (int)(kMaxScore * w / totalNote);
		return (score > kMaxScore) ? kMaxScore : score;	//満点超えてたら満点それ以外ならそのまま
	}

	//ランク基準
	struct RankInfo { int minScore; const char* image; };
	constexpr RankInfo kRanks[] =
	{
		{950000, "assets/UI/Score/S_Score.png"},
		{880000, "assets/UI/Score/A_Score.png"},
		{820000, "assets/UI/Score/B_Score.png"},
		{730000, "assets/UI/Score/C_Score.png"},
		{600000, "assets/UI/Score/D_Score.png"},
	};
	constexpr int kRankCount = sizeof(kRanks) / sizeof(kRanks[0]);

	inline const char* RankImage(int score)
	{
		for (const auto& r : kRanks)
		{
			if (score >= r.minScore) return r.image;
		}
		return kRanks[kRankCount - 1].image;
	}
}