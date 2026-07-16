#include "DebugCollision.h"

void DebugCollision::Init()
{
	m_Vertices.resize(8);	//8’¸“_

	for (auto& v : m_Vertices)
	{
		v.color = DirectX::SimpleMath::Color(1, 0, 0, 1);
		v.uv = DirectX::SimpleMath::Vector2(0, 0);
		v.normal = DirectX::SimpleMath::Vector3(0, 1, 0);
	}
	m_Indices =
	{
		0,1, 1,2, 2,3, 3,0,
		4,5, 5,6, 6,7, 7,4,
		0,4, 1,5, 2,6, 3,7
	};

	m_Shader.Create(
		"shader/unlitTextureVS.hlsl",
		"shader/unlitTexturePS.hlsl");
}

void DebugCollision::Draw(const Collision::AABB& aabb, Camera* cam)
{
	cam->SetCamera();

	DirectX::SimpleMath::Vector3 v[8] =
	{
		{aabb.min.x, aabb.min.y, aabb.min.z},
		{aabb.max.x, aabb.min.y, aabb.min.z},
		{aabb.max.x, aabb.max.y, aabb.min.z},
		{aabb.min.x, aabb.max.y, aabb.min.z},
		{aabb.min.x, aabb.min.y, aabb.max.z},
		{aabb.max.x, aabb.min.y, aabb.max.z},
		{aabb.max.x, aabb.max.y, aabb.max.z},
		{aabb.min.x, aabb.max.y, aabb.max.z},
	};

	for (int i = 0; i < 8; i++)
	{
		m_Vertices[i].position = v[i];
	}

	m_VertexBuffer.Create(m_Vertices);
	m_IndexBuffer.Create(m_Indices);

	DirectX::SimpleMath::Matrix world = DirectX::SimpleMath::Matrix::Identity;
	Renderer::SetWorldMatrix(&world);

	auto* dc = Renderer::GetDeviceContext();
	dc->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_LINELIST
	);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();

	dc->DrawIndexed(m_Indices.size(), 0, 0);
}

void DebugCollision::Uninit()
{
}
