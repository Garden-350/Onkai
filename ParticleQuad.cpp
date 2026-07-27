#include "ParticleQuad.h"
#include <iostream>

using namespace DirectX::SimpleMath;

void ParticleQuad::Init(const char* texturePath)
{
	//カメラ空間のXY平面上の正方形
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

	//テクスチャアリの単色シェーダーを流用
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

	MATERIAL mtrl{};
	mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	mtrl.TextureEnable = TRUE;
	m_Material = std::make_unique<Material>();
	m_Material->Create(mtrl);

	bool ok = m_Texture.Load(texturePath);
	if (!ok)
	{
		char buf[256];
		sprintf_s(buf, "[ParticleQuad] Texture load FAILED: %s\n", texturePath);
		OutputDebugStringA(buf);
	}
}

void ParticleQuad::Draw(Camera* cam, const Vector3& center, float scale, const Color& color)
{
	// ビルボード回転：ビュー行列の回転成分だけを取り出して常にカメラの方を向かせる
	Matrix view = cam->GetViewMatrix();
	Matrix invView = view.Invert();
	invView._41 = 0.0f; invView._42 = 0.0f; invView._43 = 0.0f; // 平行移動成分は消す

	Matrix world = Matrix::CreateScale(scale) * invView * Matrix::CreateTranslation(center);
	Renderer::SetWorldMatrix(&world);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_Material->SetDiffuse(color);
	m_Material->Update();

	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f); // UV分割なし
	m_Texture.SetGPU();
	m_Material->SetGPU();

	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}