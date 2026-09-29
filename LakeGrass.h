#pragma once
//手前の岸に生えている草とか葦
//一本一本の葉を細い板で作ってまとめて一回で描く。風でのなびきはlakeGrassVS.hlslで実装

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include <SimpleMath.h>

class Camera;
class LakeGrass
{
public:
	void Init(float shoreBaseZ);
	void Draw(Camera* cam, ID3D11Buffer* lakeCB);

private:
	void AddBlade(std::vector<VERTEX_3D>& verts, std::vector<unsigned int>& idx,
		const DirectX::SimpleMath::Vector3& base, float yaw, float leanYaw, float lean,
		float width, float height, const DirectX::SimpleMath::Vector3& color, float rnd);

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	unsigned int m_IndexCount = 0;

	//見た目調整用
	int   m_GrassCount = 20000;		// 草の本数
	int   m_ReedCount = 480;		// 葦の本数
	float m_NearZ = 30.0f;			// 草が生えている一番手前（カメラの足元）
};

