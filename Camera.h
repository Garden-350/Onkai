#pragma once

#include	<SimpleMath.h>

//-----------------------------------------------------------------------------
//Cameraクラス
//-----------------------------------------------------------------------------
class Camera {
private:
	DirectX::SimpleMath::Vector3	m_Position = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3	m_Rotation = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3	m_Scale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

	DirectX::SimpleMath::Vector3	m_Target{};
	DirectX::SimpleMath::Matrix		m_ViewMatrix{};

	float m_theta = 3.14f;	//カメラの角度(horizontal)
	float m_phi = 0.15f;	//カメラの角度(vertical)
	float m_distance = 300.0f;	//カメラの距離

	float m_targetTheta = m_theta;
	float m_targetPhi = m_phi;
	float m_targetDistance = m_distance;

	float m_minPhi = -1.2f;
	float m_maxPhi = 1.2f;
	float m_minDistance = 50.0f;
	float m_maxDistance = 2000.0f;

	float m_smooth = 0.12f;	//カメラの追従速度
	float m_CameraDirection = 0;	//カメラの距離

	bool m_Fixed = false;

public:
	void Init();
	void Update();
	void SetCamera(int mode = 0);
	void Uninit();
	void SetFixedPose(DirectX::SimpleMath::Vector3 pos, DirectX::SimpleMath::Vector3 target);

	void SetTarget(DirectX::SimpleMath::Vector3 target);

	float GetDirection() const { return m_CameraDirection; }

	const DirectX::SimpleMath::Matrix& GetViewMatrix();

	DirectX::SimpleMath::Vector3 GetPosition() const
	{
		return m_Position;
	}
};