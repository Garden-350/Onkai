#include "MagicCircleQuad.h"

using namespace DirectX::SimpleMath;

void MagicCircleQuad::Init(const char* texturePath)
{
	std::vector<VERTEX_3D> verts(4);
	verts[0].position = Vector3(-0.5f, 0.5f, 0.0f);
	verts[1].position = Vector3(0.5f, 0.5f, 0.0f);
	verts[2].position = Vector3(-0.5f, -0.5f, 0.0f);
	verts[3].position = Vector3(0.5f, -0.5f, 0.0f);
	for (int i = 0; i < 4; i++)
	{
		verts[i].normal = Vector3(0.0f, 0.0f, -1.0f);
		verts[i].color = Color(1.0f, 1.0f, 1.0f, 1.0f);
	}
	verts[0].uv = Vector2(0.0f, 0.0f);
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0,1,2,3 };
	m_IndexBuffer.Create(idx);

	//ライティングは不要  unlitTextureVS + 専用PS
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/magicCireclePS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(MagicCircleParms));

	m_Texture.Load(texturePath);
}
void MagicCircleQuad::Draw(const DirectX::SimpleMath::Vector3& center, float scale, float progress, const DirectX::SimpleMath::Vector3& glowColor, const DirectX::SimpleMath::Vector3& tintColor)
{
	Matrix world = Matrix::CreateScale(scale) * Matrix::CreateTranslation(center);
	Renderer::SetWorldMatrix(&world);

	m_Shader.SetGPU();

	MagicCircleParms parms = {};
	parms.progress = progress;
	parms.glowColor = Vector4(glowColor.x, glowColor.y, glowColor.z, 1.0f);
	parms.tintColor = Vector4(tintColor.x, tintColor.y, tintColor.z, 1.0f);
	m_Shader.UpdatePSConstantBuffer(&parms, sizeof(parms));
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);
	m_Texture.SetGPU();

	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}
