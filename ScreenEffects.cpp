#include "ScreenEffects.h"
#include <algorithm>
#include <cstdlib>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void ScreenEffects::TriggerScreenShake(float duration, float strength)
{
	//既に揺れている場合長い方を優先する
	if (duration > m_ShakeDuration - m_ShakeTimer)
	{
		m_ShakeDuration = duration;
		m_ShakeTimer = duration;
	}
	m_ShakeStrength = std::max(m_ShakeStrength, strength);
}

void ScreenEffects::TriggerFlash(float duration)
{
	m_FlashDuration = duration;
	m_FlashTimer = duration;
}

void ScreenEffects::Update(float deltaTime)
{
	//画面シェイク
	if (m_ShakeTimer > 0.0f)
	{
		m_ShakeTimer -= deltaTime;
		if (m_ShakeTimer < 0.0f) m_ShakeTimer = 0.0f;

		//残り時間の割合で強度を減衰させる(終盤ほど揺れが小さくなる)
		float t = (m_ShakeDuration > 0.0001f) ? (m_ShakeTimer / m_ShakeDuration) : 0.0f;
		float currentStrength = m_ShakeStrength * t;

		float ox = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentStrength;
		float oy = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentStrength;
		m_ShakeOffset = Vector3(ox, oy, 0.0f);

		if (m_ShakeTimer <= 0.0f)
		{
			m_ShakeStrength = 0.0f;
			m_ShakeOffset = Vector3::Zero;
		}
	}
	else
	{
		m_ShakeOffset = Vector3::Zero;
	}

	//フラッシュの更新
	if (m_FlashTimer > 0.0f)
	{
		m_FlashTimer -= deltaTime;
		if (m_FlashTimer < 0.0f) m_FlashTimer = 0.0f;
	}
}
