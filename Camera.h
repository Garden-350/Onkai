#pragma once

#include	<SimpleMath.h>

//-----------------------------------------------------------------------------
//Cameraクラス
//-----------------------------------------------------------------------------
class Camera {
public:
	enum class Layer
	{
		Playfield,	//譜面
		World		//背景
	};
	void Init();
	void Update();
	void SetCamera(int mode = 0);
	void Uninit();
	void SetFixedPose(DirectX::SimpleMath::Vector3 pos, DirectX::SimpleMath::Vector3 target);

	void SetTarget(DirectX::SimpleMath::Vector3 target);
	void SetShakeOffset(const DirectX::SimpleMath::Vector3& offset) { m_ShakeOffset = offset; }
	void SetLayer(Layer layer) { m_Layer = layer; }
	void SetWorldOffset(float yaw, float pitch, float dolly)
	{
		m_WorldYaw = yaw;
		m_WorldPitch = pitch;
		m_WorldDolly = dolly;
	}

	float GetDirection() const { return m_CameraDirection; }

	const DirectX::SimpleMath::Matrix& GetViewMatrix();
	const DirectX::SimpleMath::Matrix& GetProjectionMatrix() const { return m_ProjMatrix; }
	DirectX::SimpleMath::Vector3 GetPosition() const
	{
		return m_Position;
	}

private:
	DirectX::SimpleMath::Matrix m_ProjMatrix{};
	DirectX::SimpleMath::Vector3	m_Position = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3	m_Rotation = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3	m_Scale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

	DirectX::SimpleMath::Vector3 m_ShakeOffset = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3	m_Target{};
	DirectX::SimpleMath::Matrix		m_ViewMatrix{};
	DirectX::SimpleMath::Vector3 m_FreeMoveOffset = DirectX::SimpleMath::Vector3::Zero;

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

	float m_HeightOffset = 0.0f;
	float m_targetHeightOffset = 0.0f;
	float m_minHeightOffset = -400.0f;
	float m_maxHeightOffset = 400.0f;

	bool m_Fixed = false;

	Layer m_Layer = Layer::Playfield;
	float m_WorldYaw = 0.0f, m_WorldPitch = 0.0f, m_WorldDolly = 0.0f;

};