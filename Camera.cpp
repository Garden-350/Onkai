#include "Renderer.h"
#include "Camera.h"
#include "Application.h"
#include "input.h"
#include "Game.h"

using namespace DirectX::SimpleMath;

//=======================================
//初期化処理
//=======================================
void Camera::Init()
{
	m_Position = Vector3(0.0f, 0.0f, 0.0f);
	m_Target = Vector3(0.0f, 0.0f, 0.0f);
	m_CameraDirection = 3.14f;
}


//=======================================
//更新処理
//=======================================
void Camera::Update()
{
	//左右キーでカメラ回転
	if (Input::GetKeyPress(VK_L))
	{
		m_CameraDirection += 0.02f;
	}
	if (Input::GetKeyPress(VK_J))
	{
		m_CameraDirection -= 0.02f;
	}
	
	Vector3 pos = m_Target;
	pos.x += sin(m_CameraDirection) * 50;
	pos.y += 20;
	pos.z += cos(m_CameraDirection) * 50;
	m_Position = pos;
}

//=======================================
//描画処理
//=======================================
void Camera::SetCamera(int mode)
{
	//3D
	if (mode == 0)
	{
		// ビュー変換後列作成
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		m_ViewMatrix = DirectX::XMMatrixLookAtLH(m_Position, m_Target, up); //左手系

		Renderer::SetViewMatrix(&m_ViewMatrix);

		//プロジェクション行列の生成
		constexpr float fieldOfView = DirectX::XMConvertToRadians(45.0f);    // 視野角

		float aspectRatio = static_cast<float>(Application::GetWidth()) / static_cast<float>(Application::GetHeight());	// アスペクト比	
		float nearPlane = 1.0f;       // ニアクリップ 0はダメ（0割りを起こす）
		float farPlane = 1000.0f;      // ファークリップ

		//プロジェクション行列の生成
		Matrix projectionMatrix;
		projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, aspectRatio, nearPlane, farPlane);	//左手系

		Renderer::SetProjectionMatrix(&projectionMatrix);
	}
	//2D
	else if (mode == 1)
	{
		// ビュー変換後列作成
		Vector3 pos = Vector3(0.0f, 0.0f, -10.0f);
		Vector3 tgt = Vector3(0.0f, 0.0f, 1.0f);
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		m_ViewMatrix = DirectX::XMMatrixLookAtLH(pos, tgt, up); //左手系

		Renderer::SetViewMatrix(&m_ViewMatrix);

		//プロジェクション行列の生成
		float nearPlane = 1.0f;       // ニアクリップ 0はダメ（0割りを起こす）
		float farPlane = 1000.0f;      // ファークリップ

		//プロジェクション行列の生成
		Matrix projectionMatrix = DirectX::XMMatrixOrthographicLH(
		static_cast<float>(Application::GetWidth()),
			static_cast<float>(Application::GetHeight()),
		nearPlane, farPlane);
		projectionMatrix = DirectX::XMMatrixTranspose(projectionMatrix);	//左手系

		Renderer::SetProjectionMatrix(&projectionMatrix);
	}
}


//=======================================
//終了処理
//=======================================
void Camera::Uninit()
{

}

void Camera::SetTarget(DirectX::SimpleMath::Vector3 target)
{
	//カメラの注視点を更新
	m_Target = target;
}

