#pragma once
#include "Object.h"
#include "MagicCircleQuad.h"

// 曲決定時の遷移演出専用：ジャケットの上に魔法陣を左右から描いていく
class MagicCircleTransitionEffect : public Object
{
private:
	MagicCircleQuad m_Quad;
	float m_Progress = 0.0f;
	float m_QuadScale = 260.0f;
	bool m_Visible = false;
	DirectX::SimpleMath::Vector3 m_GlowColor = DirectX::SimpleMath::Vector3(0.8f, 0.5f, 1.0f);
	DirectX::SimpleMath::Vector3 m_TintColor = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

public:
	void Init() override;
	void Update() override {}
	void Draw(Camera* cam) override;
	void Uninit() override {}

	void SetProgress(float progress) { m_Progress = progress; }
	void SetVisible(bool visible) { m_Visible = visible; }
	void SetScale(float scale) { m_QuadScale = scale; }
};