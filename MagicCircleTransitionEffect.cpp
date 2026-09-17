#include "MagicCircleTransitionEffect.h"

void MagicCircleTransitionEffect::Init()
{
	m_Quad.Init("assets/texture/magic-circle.png", "shader/magicCircleTransitionPS.hlsl");
}

void MagicCircleTransitionEffect::Draw(Camera* cam)
{
	if (!m_Visible) return;

	cam->SetCamera(1);
	m_Quad.Draw(DirectX::SimpleMath::Vector3(0.0f, 0.0f, 200.0f), m_QuadScale, m_Progress, m_GlowColor, m_TintColor);
}