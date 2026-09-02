#pragma once
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include <SimpleMath.h>

class BarrierQuad
{
private:
	struct BarrierParms
	{
		float progress;
		float padding0[3];
		DirectX::SimpleMath::Vector4 baseColor;
	};	//sizeof == 32

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

public:
	void Init();
	void Draw(const DirectX::SimpleMath::Vector3& center, float scale,
		float progress, const DirectX::SimpleMath::Vector3& baseColor);
};

