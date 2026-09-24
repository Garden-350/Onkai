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
	m_theta = 3.14f;
	m_phi = 0.15f;
	m_distance = 300.0f;
	m_targetTheta = m_theta;
	m_targetPhi = m_phi;
	m_targetDistance = m_distance;
	m_CameraDirection = m_theta;
}


//=======================================
//更新処理（カメラ操作キー：左右=回転、上下=ピッチ、PageUp/PageDown=ズーム、C=固定切替）
//=======================================
void Camera::Update()
{
	//トグル　Cキーで固定と操作切替
	if (Input::GetKeyTrigger(VK_C))
	{
		m_Fixed = !m_Fixed;
	}
	if (m_Fixed) return; // 固定モード中はオービット計算をスキップ
	//左右キーでカメラ回転
	//操作入力
	const float rotSpeed = 0.04f;
	const float heightSpeed = 6.0f;
	const float zoomSpeed = 25.0f;
	const float cameraSpeed = 5.0f;

	if (Input::GetKeyPress(VK_LEFT))
	{
		m_targetTheta -= rotSpeed;
	}
	if (Input::GetKeyPress(VK_RIGHT))
	{
		m_targetTheta += rotSpeed;
	}
	if (Input::GetKeyPress(VK_UP))
	{
		m_targetHeightOffset += heightSpeed;
	}
	if (Input::GetKeyPress(VK_DOWN))
	{
		m_targetHeightOffset -= heightSpeed;
	}
	if (Input::GetKeyPress(VK_PRIOR)) // PageUp
	{
		m_targetDistance -= zoomSpeed;
	}
	if (Input::GetKeyPress(VK_NEXT)) // PageDown
	{
		m_targetDistance += zoomSpeed;
	}
	if (Input::GetKeyPress(VK_J))
	{
		m_FreeMoveOffset.x -= cameraSpeed;
	}
	if (Input::GetKeyPress(VK_L))
	{
		m_FreeMoveOffset.x += cameraSpeed;
	}
	if (Input::GetKeyPress(VK_I))
	{
		m_FreeMoveOffset.y += cameraSpeed;
	}
	if (Input::GetKeyPress(VK_K))
	{
		m_FreeMoveOffset.y -= cameraSpeed;
	}
	if (Input::GetKeyPress(VK_P))
	{
		m_FreeMoveOffset.z += cameraSpeed;
	}
	if (Input::GetKeyPress(VK_O))
	{
		m_FreeMoveOffset.z -= cameraSpeed;
	}

	//clamp
	if (m_targetHeightOffset < m_minHeightOffset) m_targetHeightOffset = m_minHeightOffset;
	if (m_targetHeightOffset > m_maxHeightOffset) m_targetHeightOffset = m_maxHeightOffset;
	if (m_targetDistance < m_minDistance) m_targetDistance = m_minDistance;
	if (m_targetDistance > m_maxDistance) m_targetDistance = m_maxDistance;

	//smooth interp
	m_theta += (m_targetTheta - m_theta) * m_smooth;
	m_HeightOffset += (m_targetHeightOffset - m_HeightOffset) * m_smooth;
	m_distance += (m_targetDistance - m_distance) * m_smooth;

	//compute position from spherical coords(左右) + 高さオフセット(上下)
	float cosPhi = cosf(m_phi);
	float x = m_Target.x + m_distance * cosPhi * sinf(m_theta);
	float y = m_Target.y + m_distance * sinf(m_phi) + m_HeightOffset; // 高さオフセットを加算
	float z = m_Target.z + m_distance * cosPhi * cosf(m_theta);
	m_Position = Vector3(x, y, z) + m_FreeMoveOffset;
	m_CameraDirection = m_theta;
}

//=======================================
//描画処理
//=======================================
void Camera::SetCamera(int mode)
{
	//3D
	if (mode == 0)
	{
		Vector3 shakenPos = m_Position + m_ShakeOffset;
		Vector3 shakenTarget = m_Target + m_ShakeOffset;
		// ビュー変換行列作成
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		m_ViewMatrix = DirectX::XMMatrixLookAtLH(shakenPos, shakenTarget, up); //左手系
		Renderer::SetViewMatrix(&m_ViewMatrix);
		//プロジェクション行列の生成
		constexpr float fieldOfView = DirectX::XMConvertToRadians(45.0f); // 視野角

		float aspectRatio = static_cast<float>(Application::GetWidth()) / static_cast<float>(Application::GetHeight());	// アスペクト比	
		float nearPlane = 1.0f; // ニアクリップ 0はダメ（0割りを起こす）
		float farPlane = 3000.0f; // ファークリップ

		//プロジェクション行列の生成
		Matrix projectionMatrix;
		projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, aspectRatio, nearPlane, farPlane);	//左手系

		Renderer::SetProjectionMatrix(&projectionMatrix);
	}
	//2D
	else if (mode == 1)
	{
		// ビュー変換行列作成
		Vector3 pos = Vector3(0.0f, 0.0f, -10.0f);
		Vector3 tgt = Vector3(0.0f, 0.0f, 1.0f);
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		m_ViewMatrix = DirectX::XMMatrixLookAtLH(pos, tgt, up); //左手系

		Renderer::SetViewMatrix(&m_ViewMatrix);

		//プロジェクション行列の生成
		float nearPlane = 1.0f; // ニアクリップ 0はダメ（0割りを起こす）
		float farPlane = 1000.0f; // ファークリップ

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
	std::cout << "CameraPosition : " << m_Position.x << "," << m_Position.y << "," << m_Position.z << std::endl;
}

void Camera::SetFixedPose(DirectX::SimpleMath::Vector3 pos, DirectX::SimpleMath::Vector3 target)
{
	m_Position = pos;
	m_Target = target;
	m_Fixed = true;
	// derive spherical params from provided pose so toggle/controls are smooth
	DirectX::SimpleMath::Vector3 d = m_Position - m_Target;
	m_distance = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
	if (m_distance <= 0.0001f) m_distance = 1.0f;
	m_phi = std::asin(d.y / m_distance);
	m_theta = std::atan2(d.x, d.z);
	m_targetTheta = m_theta;
	m_targetPhi = m_phi;
	m_targetDistance = m_distance;
	m_CameraDirection = m_theta;
}

void Camera::SetTarget(DirectX::SimpleMath::Vector3 target)
{
	//カメラの注視点を更新
	m_Target = target;
}

const DirectX::SimpleMath::Matrix& Camera::GetViewMatrix()
{
	return m_ViewMatrix;
}

