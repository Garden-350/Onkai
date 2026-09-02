#pragma once
#include "Object.h"
#include "Renderer.h"
#include "Shader.h"

class WireShape : public Object
{
private:
	struct LineVertex {
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector3 normal;
		DirectX::SimpleMath::Color color;
		DirectX::SimpleMath::Vector2 tex;
	};
	ID3D11Buffer* m_pVertexBuffer = nullptr;
	Shader m_Shader;
	int m_VertexCount = 0;

	float m_X = 0.0f;
	float m_Y = 0.0f;
	float m_Z = 0.0f;
	float m_RotX = 0.0f;
	float m_RotY = 0.0f;
	float m_RotZ = 0.0f;
	float m_Width = 100.0f;
	float m_Height = 100.0f;
	float m_ScrollSpeed = 300.0f;
	DirectX::SimpleMath::Color m_Color = { 0,1,1,1 };

public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	void SetUp(float x, float y, float z, float w, float h,
		DirectX::SimpleMath::Color col, float speed);
	void SetRot(float x, float y, float z) { m_RotX = x, m_RotY = y, m_RotZ = z; }
	bool IsOutOfScreen() const { return m_Z < -500.0f; }
};

