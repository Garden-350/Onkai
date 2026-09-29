#include "ScreenFade.h"
#include "Camera.h"
#include "Application.h"

using namespace DirectX::SimpleMath;

void ScreenFade::Init()
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

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/screenFadePS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(Vector4));
}

void ScreenFade::Draw(Camera* cam)
{
	if (m_Alpha <= 0.001f) return;	// 見えないときは何もしない

	cam->SetCamera(1);	// 2Dカメラ（画面の中心が原点、1単位=1ピクセル）
	Matrix world = Matrix::CreateScale((float)Application::GetWidth(), (float)Application::GetHeight(), 1.0f);
	Renderer::SetWorldMatrix(&world);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);

	Renderer::SetBlendState(BS_ALPHABLEND);
	Renderer::SetDepthOverlayEnable(true);	// 深度テストも書き込みもしない（何があっても一番手前）
	Renderer::SetCullMode(CULL_NONE);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	float a = m_Alpha > 1.0f ? 1.0f : m_Alpha;
	Vector4 color(m_FadeColor.x, m_FadeColor.y, m_FadeColor.z, a);
	m_Shader.UpdatePSConstantBuffer(&color, sizeof(color));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthOverlayEnable(false);
	cam->SetCamera(0);	// 後続の3D描画のために3Dカメラへ戻しておく
}
