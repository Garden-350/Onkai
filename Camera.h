#pragma once

#include	<SimpleMath.h>

//-----------------------------------------------------------------------------
//CameraƒNƒ‰ƒX
//-----------------------------------------------------------------------------
class Camera {
private:
	DirectX::SimpleMath::Vector3	m_Position = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3	m_Rotation = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3	m_Scale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

	DirectX::SimpleMath::Vector3	m_Target{};
	DirectX::SimpleMath::Matrix		m_ViewMatrix{};

	float m_CameraDirection = 0;	//ƒJƒƒ‰‚Ì•ûŒü


public:
	void Init();
	void Update();
	void SetCamera(int mode = 0);
	void Uninit();

	void SetTarget(DirectX::SimpleMath::Vector3 target);

	float GetDirection() const { return m_CameraDirection; }
};