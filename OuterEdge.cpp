#include "OuterEdge.h"
#include "Camera.h"
#include "Application.h"
#include <cmath>
#undef min
#undef max
using namespace DirectX::SimpleMath;

namespace
{
	constexpr float kPi = 3.14159265f;
	constexpr float kDegToRad = kPi / 180.0f;
	constexpr float kCameraFovDeg = 45.0f;	// Camera::SetCamera(0) と同じ視野角

	// 2色を交互に使う
	const Vector3 kColorA(0.35f, 0.70f, 1.00f);	// 青
	const Vector3 kColorB(1.00f, 0.00f, 0.00f);	// 赤

	constexpr float kPulseWidth = 0.12f;	// 走る光の長さ（列全体を1としたときの幅）
	constexpr float kDownbeatFlash = 0.15f;	// 拍の頭で全体が一瞬明るくなる量（0で無し）

	Vector3 Lerp3(const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; }
}

void OuterEdge::Init()
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
	verts[0].uv = Vector2(0.0f, 0.0f);	// uv.y=0 が柱の先端(上)
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);	// uv.y=1 が柱の根本(下)
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/spotlightPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(OuterEdgeParams));

	auto rnd = []() { return (float)rand() / (float)RAND_MAX; };
	for (int i = 0; i < m_LightsPerSide; i++)
	{
		Orb out;
		float z = -600.0f + rnd() * 800.0f;
		float t = (z + 600.0f) / 800.0f;
		float rowX = 530.0f + (330.0f - 530.0f) * t;
		float side = (rnd() < 0.5f) ? -1.0f : 1.0f;
		out.pos = Vector3(side * (rowX + 60.0f + rnd() * 340.0f), -300.0f + rnd() * 70.0f, z);
		out.color = Vector3(0.0f, 1.0f, 0.0f);
		out.size = 140.0f + rnd() * 80.0f;
		out.phase = rnd() * 6.28f;	//位相
		out.speed = 0.5f + rnd();
		m_Orbs.push_back(out);
	}
}

void OuterEdge::Update()
{
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted)
	{
		m_LastClock = now;
		m_ClockStarted = true;
	}
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	if (dt > 0.1f) dt = 0.1f;	// ポーズ明けなどで大きく飛ばないように
	m_Time += dt;
}

void OuterEdge::DrawQuad(const Matrix& world, const OuterEdgeParams& params)
{
	Matrix w = world;
	Renderer::SetWorldMatrix(&w);
	m_Shader.UpdatePSConstantBuffer(&params, sizeof(params));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}

void OuterEdge::Draw(Camera* cam)
{
	cam->SetCamera(0);
	const Matrix invView = cam->GetViewMatrix().Invert();
	const Vector3 camPos(invView._41, invView._42, invView._43);
	const Vector3 camRight(invView._11, invView._12, invView._13);
	const Vector3 camUp(invView._21, invView._22, invView._23);
	Renderer::SetBlendState(BS_ADDITIVE);
	Renderer::SetDepthEnable(false);	// 深度は書き込まない（後から描くノーツ等を邪魔しない）
	Renderer::SetCullMode(CULL_NONE);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);
	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	
	
	const Vector3 farLeft(m_FarTarget.x - 300.0f, -40.0f, m_FarTarget.z);
	const Vector3 farRight(m_FarTarget.x + 300.0f, -40.0f, m_FarTarget.z);
	for (const Orb& o : m_Orbs)
	{
		float bob = sinf(m_Time * o.speed + o.phase) * 20.0f;
		float intensity = 0.5f + 0.4f * sinf(m_Time * 2.0f + o.phase * 3.0f);
		Vector3 base = o.pos + Vector3(0.0f, bob, 0.0f);
		float size = o.size;

		Vector3 normal = camPos - base;
		normal.Normalize();
		Matrix world(
			camRight.x * size, camRight.y * size, camRight.z * size, 0.0f,
			camUp.x * size, camUp.y * size, camUp.z * size, 0.0f,
			normal.x, normal.y, normal.z, 0.0f,
			base.x, base.y, base.z, 1.0f);

		OuterEdgeParams outer = {};
		outer.color = Vector4(o.color.x, o.color.y, o.color.z, intensity);
		outer.mode = 1.0f;
		DrawQuad(world, outer);
	}

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_NONE);
}
