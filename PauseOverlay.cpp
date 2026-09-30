#include "PauseOverlay.h"
#include "Game.h"

using namespace DirectX::SimpleMath;

void PauseOverlay::Init()
{
	m_Fade.Init();
	m_Fade.SetColor(Vector3(1.0f, 1.0f, 1.0f));
	m_Fade.SetAlpha(0.5f);

	m_TextRetry.Init();
	m_TextRetry.SetTexture("assets/texture/retry.png");
	m_TextRetry.SetPosition(-300.0f, 0.0f, 0.0f);
	m_TextRetry.SetRotation(0.0f, 0.0f, 0.0f);
	m_TextRetry.SetScale(300.0f, 300.0f, 0.0f);

	m_TextGo.Init();
	m_TextGo.SetTexture("assets/texture/Go.png");
	m_TextGo.SetPosition(300.0f, 0.0f, 0.0f);
	m_TextGo.SetRotation(0.0f, 0.0f, 0.0f);
	m_TextGo.SetScale(300.0f, 300.0f, 0.0f);
}

void PauseOverlay::Draw(Camera* cam)
{
	if (!Game::GetInstance()->IsPaused()) return;

	m_Fade.Draw(cam);
	m_TextRetry.Draw(cam);
	m_TextGo.Draw(cam);
	cam->SetCamera(0);
}
