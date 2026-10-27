#include "NumberDisplay.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

void NumberDisplay::Init()
{
	for (auto& d : m_Digits)d.Init();
	m_ClockStarted = false;
}

void NumberDisplay::Update()
{
	//自分で時間を測る
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted) { m_LastClock = now; m_ClockStarted = true; }
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	dt = std::min(dt, 0.1f);	//ポーズ明けに一気に飛ばないように

	double diff = m_Value - m_Shown;
	if (std::fabs(diff) < 1.0f)m_Shown = m_Value;
	else m_Shown += diff * std::min(1.0f, m_CountSpeed * dt);
}

void NumberDisplay::Draw(Camera* cam)
{
	if (m_DigitCount <= 0 || m_Alpha <= 0.0f) return;

	//表示する値を一桁ずつに分ける（左が上の桁）
	int digits[kMaxDigits];
	int v = (int)std::llround(m_Shown);
	for (int i = m_DigitCount - 1; i >= 0; i--)
	{
		digits[i] = v % 10;
		v /= 10;
	}

	//左から見て最初の0でない桁。それより左は「頭の0」（値が0の時は一番右だけ普通に出す）
	int firstNonZero = m_DigitCount - 1;
	for (int i = 0; i < m_DigitCount; i++)
	{
		if (digits[i] != 0) { firstNonZero = i; break; }
	}
	Renderer::SetBlendState(BS_ALPHABLEND);
	float left = m_X - m_Spacing * (m_DigitCount - 1) * 0.5f;
	for (int i = 0; i < m_DigitCount; i++)
	{
		float a = ((i < firstNonZero) ? m_LeadingZeroAlpha : 1.0f) * m_Alpha;
		if (a <= 0.0003f) continue;

		Texture2D& t = m_Digits[i];
		t.SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
		t.SetPosition(left + m_Spacing * i, m_Y, m_Z);
		t.SetScale(m_W, m_H, 1.0f);
		t.SetAlpha(a);
		t.Draw(cam);
	}
}

void NumberDisplay::SetUp(const char* texPath, int digits)
{
	m_DigitCount = std::clamp(digits, 1, kMaxDigits);
	for (int i = 0; i < m_DigitCount; i++)
	{
		m_Digits[i].SetTexture(texPath);
		m_Digits[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f);
	}
}

void NumberDisplay::SetValue(int value, bool instant)
{
	//桁数に入り切る最大値でストップ
	int limit = 1;
	for (int i = 0; i < m_DigitCount; i++) limit *= 10;
	m_Value = std::clamp(value, 0, limit - 1);
	if (instant) m_Shown = m_Value;
}
