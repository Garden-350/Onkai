#include "HitPillarEffect.h"
#include "Camera.h"

using namespace DirectX::SimpleMath;

void HitPillarEffect::Init()
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
	verts[0].uv = Vector2(0.0f, 0.0f);	//uv.y=0 が柱の先端
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);	//uv.y=1 が柱の根本
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	//スポットライトと同じシェーダーを使う（末広がりの光の柱＋根本の丸い光）
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/spotlightPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(PillarParams));
}

void HitPillarEffect::DrawQuad(const Matrix& world, const PillarParams& params)
{
	Matrix w = world;
	Renderer::SetWorldMatrix(&w);
	m_Shader.UpdatePSConstantBuffer(&params, sizeof(params));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}

void HitPillarEffect::Draw(Camera* cam,
	const Vector3& base, const Vector3& dir,
	float length, float baseWidth, float topWidth,
	const Vector3& color, float intensity, float lampSize)
{
	if (intensity <= 0.001f || length <= 0.01f || topWidth <= 0.01f) return;

	const Matrix invView = cam->GetViewMatrix().Invert();
	const Vector3 camRight(invView._11, invView._12, invView._13);
	const Vector3 camUp(invView._21, invView._22, invView._23);
	const Vector3 camPos(invView._41, invView._42, invView._43);

	Renderer::SetDepthOverlayEnable(true);	// 深度テストも書き込みもしない
	Renderer::SetCullMode(CULL_NONE);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//光の柱：軸(dir)まわりだけカメラの方へ向ける
	{
		Vector3 axis = dir;
		axis.Normalize();
		Vector3 mid = base + axis * (length * 0.5f);	//板の中心は根本から長さの半分だけ先
		Vector3 side = axis.Cross(camPos - mid);
		if (side.LengthSquared() < 0.0001f) side = camRight;
		side.Normalize();
		Vector3 normal = side.Cross(axis);
		normal.Normalize();

		Matrix world(
			side.x * topWidth, side.y * topWidth, side.z * topWidth, 0.0f,
			axis.x * length, axis.y * length, axis.z * length, 0.0f,
			normal.x, normal.y, normal.z, 0.0f,
			mid.x, mid.y, mid.z, 1.0f);

		PillarParams p = {};
		p.color = Vector4(color.x, color.y, color.z, intensity);
		p.mode = 0.0f;
		p.baseWidthRatio = baseWidth / topWidth;
		DrawQuad(world, p);
	}

	//根本の丸い光：完全にカメラの方を向く板
	if (lampSize > 0.01f)
	{
		Vector3 normal = camPos - base;
		normal.Normalize();
		Matrix world(
			camRight.x * lampSize, camRight.y * lampSize, camRight.z * lampSize, 0.0f,
			camUp.x * lampSize, camUp.y * lampSize, camUp.z * lampSize, 0.0f,
			normal.x, normal.y, normal.z, 0.0f,
			base.x, base.y, base.z, 1.0f);

		PillarParams p = {};
		p.color = Vector4(color.x, color.y, color.z, intensity);
		p.mode = 1.0f;
		DrawQuad(world, p);
	}

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthOverlayEnable(false);	// 通常の深度テストに戻す
}
