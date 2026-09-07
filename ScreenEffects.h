#pragma once
#include <SimpleMath.h>

// ============================================================
// ScreenEffects : 画面シェイクとフラッシュ演出だけを管理するクラス。
//   ChartObjectの譜面/判定状態には一切依存しない、独立した小さな状態機械。
//   （ChartObject::TriggerScreenShake/TriggerFlash とUpdate内の減衰処理を
//     丸ごとここに移設したもの）
// ============================================================
class ScreenEffects
{
public:
	void TriggerScreenShake(float duration, float strength);
	void TriggerFlash(float duration);

	// 毎フレーム呼ぶ：シェイク・フラッシュの残り時間を減衰させる
	void Update(float deltaTime);

	DirectX::SimpleMath::Vector3 GetShakeOffset() const { return m_ShakeOffset; }

	float GetFlashAlpha() const
	{
		return (m_FlashDuration > 0.0001f) ? (m_FlashTimer / m_FlashDuration) : 0.0f;
	}

private:
	DirectX::SimpleMath::Vector3 m_ShakeOffset = DirectX::SimpleMath::Vector3::Zero;
	float m_ShakeTimer = 0.0f;
	float m_ShakeDuration = 0.0f;
	float m_ShakeStrength = 0.0f;
	float m_FlashTimer = 0.0f;
	float m_FlashDuration = 0.0f;
};
