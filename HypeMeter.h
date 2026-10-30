#pragma once

class HypeMeter
{
public:
	static constexpr int kStages = 4;
	//段階の数
	static int Threshold(int i)	//何コンボで次の段階に移るか
	{
		static const int t[kStages] = { 10,30,60,100 };
		return t[i];
	}
	static constexpr float kRiseSpeed = 12.0f;	//上がる速さ(１秒に何段階)
	static constexpr float kFallSpeed = 0.25f;	//下がる速さ(0.25の場合　一番上から0までは16秒)

	void Reset() { m_Level = 0.0f; m_Stage = 0; }

	//毎フレーム呼ぶ
	bool Update(float deltaTime, int combo)
	{
		int stage = 0;
		for (int i = 0; i < kStages; i++)
		{
			if (combo >= Threshold(i)) stage++;
		}
		bool stageUp = (stage > m_Stage);
		m_Stage = stage;

		//目標の段階へ、上がる速度は速く下がる速度はゆっくり
		float target = (float)stage;
		if (m_Level < target)
		{
			m_Level += kFallSpeed * deltaTime;
			if (m_Level < target) m_Level = target;
		}
		else
		{
			m_Level -= kFallSpeed * deltaTime;
			if (m_Level < target) m_Level = target;
		}
		return stageUp;
	}

	float GetLevel() const { return m_Level; }
	//0~kStages
	float Get01() const { return m_Level / (float)kStages; }
	int GetStage() const { return m_Stage; }

private:
	float m_Level = 0.0f;	//なめらかに動く盛り上がり度
	int m_Stage = 0;		//コンボから決まる段階
};