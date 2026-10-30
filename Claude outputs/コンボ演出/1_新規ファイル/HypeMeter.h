#pragma once

// ============================================================
// HypeMeter : コンボから「盛り上がり度」を作る
//   ・コンボが Threshold の数を超えるたびに、段階が1つ上がる（0 ～ kStages）
//   ・上がる時は速く（kRiseSpeed）、ミスで下がる時はゆっくり（kFallSpeed）
//     ミスでコンボが 0 になっても、盛り上がりは少しずつしか下がらない。
//     その間にまたつなげば、下がるのはそこで止まる
//   ・Get01() で 0～1 の値を返す。背景・観客・曲の音の明るさに使う
// ============================================================
class HypeMeter
{
public:
	// ---- 調整用 ----
	static constexpr int   kStages = 4;							// 段階の数
	static int Threshold(int i)	// 何コンボで次の段階か（ここの数字を変える）
	{
		static const int t[kStages] = { 10, 30, 60, 100 };
		return t[i];
	}
	static constexpr float kRiseSpeed = 2.0f;	// 上がる速さ（1秒に何段階）
	static constexpr float kFallSpeed = 0.25f;	// 下がる速さ（1秒に何段階。0.25 なら一番上から 0 まで16秒）

	void Reset() { m_Level = 0.0f; m_Stage = 0; }

	// 毎フレーム呼ぶ。段階が上がった瞬間だけ true を返す（演出を足したくなった時用）
	bool Update(float deltaTime, int combo)
	{
		int stage = 0;
		for (int i = 0; i < kStages; i++)
		{
			if (combo >= Threshold(i)) stage++;
		}
		bool stageUp = (stage > m_Stage);
		m_Stage = stage;

		// 目標の段階へ、上がる時は速く・下がる時はゆっくり近づける
		float target = (float)stage;
		if (m_Level < target)
		{
			m_Level += kRiseSpeed * deltaTime;
			if (m_Level > target) m_Level = target;
		}
		else
		{
			m_Level -= kFallSpeed * deltaTime;
			if (m_Level < target) m_Level = target;
		}
		return stageUp;
	}

	float GetLevel() const { return m_Level; }						// 0 ～ kStages
	float Get01() const { return m_Level / (float)kStages; }		// 0 ～ 1
	int   GetStage() const { return m_Stage; }						// コンボから決まる今の段階

private:
	float m_Level = 0.0f;	// なめらかに動く盛り上がり度
	int   m_Stage = 0;		// コンボから決まる段階
};
