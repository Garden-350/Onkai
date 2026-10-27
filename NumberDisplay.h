#pragma once
#include "Object.h"
#include "Texture2D.h"
#include <chrono>

class NumberDisplay : public Object
{
public:
	void Init() override;
	void Update() override;	//カウントアップ
	void Draw(Camera* cam) override;
	void Uninit() override {};

	//texPath:数字の画像　digits:桁数
	void SetUp(const char* texPath, int digits);

	//instant = trueで切替
	void SetValue(int value, bool instant = false);
	int GetValue() const { return m_Value; }

	void SetPosition(float x, float y, float z = 0.0f) { m_X = x; m_Y = y; m_Z = z; }	//数字の列の中心
	void SetDigitSize(float w, float h) { m_W = w; m_H = h; m_Spacing = w * 0.75; }		//一桁の大きさ
	void SetSpacing(float s) { m_Spacing = s; }	//桁間隔
	void SetLeadingZeroAlpha(float a) { m_LeadingZeroAlpha = a; }	//0~1で薄くから通常に
	void SetAlpha(float a) { m_Alpha = a; }
	void SetCountSpeed(float s) { m_CountSpeed = s; }	//大きいほど速く

private:
	static constexpr int kMaxDigits = 9;

	Texture2D m_Digits[kMaxDigits];
	int m_DigitCount = 0;
	int m_Value = 0;	//値
	double m_Shown = 0.0f;	//表示してる値(m_Valueを追いかける)

	float m_X = 0.0f, m_Y = 0.0f, m_Z = 0.0f;
	float m_W = 40.0f, m_H = 40.0f;
	float m_Spacing = 30.0f;
	float m_LeadingZeroAlpha = 0.25f;
	float m_Alpha = 1.0f;
	float m_CountSpeed = 10.0f;

	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;
};

