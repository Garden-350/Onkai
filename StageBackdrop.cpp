#include "StageBackdrop.h"
#include "Renderer.h"
#include "Camera.h"
#include "Application.h"
#include "ChargeAura.h"

void StageBackdrop::Init()
{
	m_Image.Init();
	m_Image.SetTexture("assets/texture/skyboxTexture.png");
	m_Image.SetPosition(0.0f, 0.0f, 500.0f);	// 2Dカメラのニア/ファーの内側ならどこでもOK
	m_Image.SetScale((float)Application::GetWidth(), (float)Application::GetHeight(), 1.0f);
}

void StageBackdrop::Draw(Camera* cam)
{
	Renderer::SetBlendState(BS_NONE);
	Renderer::SetDepthOverlayEnable(true);	// 深度テストも書き込みもしない
	m_Image.Draw(cam);						// 中で2Dカメラに切り替わる
	ChargeAura::Instance().Draw(cam);		//必殺技チャージ中の金背景描画
	Renderer::SetDepthOverlayEnable(false);
	Renderer::SetBlendState(BS_ALPHABLEND);
	cam->SetCamera(0);						// 後続の3D描画のために3Dカメラへ戻しておく
}
