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

	//コンボが少ない時
	const Vector3 kBaseColor(0.2f, 1.0f, 0.45f);

	//盛り上がった時の色とりどりの色（ここに足すと色が増える）
	const Vector3 kPalette[] =
	{
		Vector3(1.0f, 0.3f, 0.7f),	//ピンク
		Vector3(0.3f, 0.85f, 1.0f),	//水色
		Vector3(1.0f, 0.9f, 0.3f),	//黄色
		Vector3(0.65f, 0.4f, 1.0f),	//紫
		Vector3(1.0f, 0.55f, 0.2f),	//オレンジ
		Vector3(1.0f, 1.0f, 1.0f),	//白
	};
	constexpr int kPaletteCount = sizeof(kPalette) / sizeof(kPalette[0]);

	float Rand01() { return (float)rand() / (float)RAND_MAX; }
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
	m_Shader.CreatePSConstantBuffer(sizeof(Params));

	m_Lights.clear();
	for (int i = 0; i < kCount; i++)
	{
		Penlight p;
		float z = -600.0f + Rand01() * 800.0f;
		float t = (z + 600.0f) / 800.0f;
		float rowX = 530.0f + (330.0f - 530.0f) * t;	//置くほどレーンが細いので客席も内へ寄せる
		float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
		p.pos = Vector3(side * (rowX + 60.0f + Rand01() * 340.0f), -300.0f + Rand01() * 70.0f, z);
		p.size = 0.8f + Rand01() * 0.4f;
		p.phase = Rand01() * 6.28f;
		p.speed = 0.5f + Rand01();
		p.order = Rand01();
		p.colorPick = Rand01();
		p.colorIndex = (int)(Rand01() * kPaletteCount) % kPaletteCount;
		p.swingJitter = (Rand01() - 0.5f) * 0.6f;	//+-0.3拍
		m_Lights.push_back(p);
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

void OuterEdge::Draw(Camera* cam)
{
	cam->SetCamera(0);
	const Matrix invView = cam->GetViewMatrix().Invert();
	const Vector3 camPos(invView._41, invView._42, invView._43);
	const Vector3 camRight(invView._11, invView._12, invView._13);
	const Vector3 camUp(invView._21, invView._22, invView._23);

	Renderer::SetBlendState(BS_ADDITIVE);
	Renderer::SetDepthEnable(false);	//深度は書き込まない
	Renderer::SetCullMode(CULL_NONE);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);
	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//拍
	float beatPos = (m_HasBeat && m_Bpm > 0.0f && m_MusicTime >= 0.0f) ? m_MusicTime * m_Bpm / 60.0f : m_Time * 2.0f;
	float beatFrac = beatFrac = beatPos - floorf(beatPos);
	float flash = expf(-beatFrac * 6.0f) * kBeatFlash * m_Hype;	//拍の頭で一瞬明るくする

	//盛り上がり時に変わるもの
	float visible = kMinVisible + (1.0f - kMinVisible) * m_Hype;	//出ている割合
	float bright = kMinBright + (1.0f - kMinBright) * m_Hype;		//明るさ
	float sync = m_Hype;	//振りの揃い具合

	for (const Penlight& p : m_Lights)
	{
		//出てくる：orderがvisibleを超えているのはまだ振ってない
		float appear = (visible - p.order) / 0.05f;	//ふわっと
		if (appear <= 0.0f) continue;
		if (appear > 1.0f) appear = 1.0f;
		//振り：cos(π*拍)　拍ちょうどで+1/-1(左右の端)そろってないのは少し遅れる
		float b = beatPos + p.swingJitter * (1.0f - sync);
		float c = cosf(kPi * b);
		float swing = (c >= 0.0f ? 1.0f : -1.0f) * powf(fabsf(c), 0.8f);	//端ですこし溜め
		float ang = swing * kSwingDeg * kDegToRad;
		Vector3 dir = camUp * cosf(ang) + camRight * sinf(ang);	//持ちて->先の向き
		dir.Normalize();

		float bob = sinf(m_Time * p.speed + p.phase) * 8.0f;
		Vector3 hand = p.pos + Vector3(0.0f, bob, 0.0f);
		float len = kStickLength * p.size;
		Vector3 tip = hand + dir * len;

		//色
		Vector3 color = (p.colorPick < m_Hype) ? kPalette[p.colorIndex] : kBaseColor;
		float intensity = (bright + flash) * appear;

		//棒
		{
			Vector3 axis = -dir;
			Vector3 mid = tip + axis * (len * 0.5f);
			Vector3 toCam = camPos - mid;
			Vector3 sideVec = axis.Cross(toCam);
			if (sideVec.LengthSquared() < 0.0001f) sideVec = camRight;
			sideVec.Normalize();
			Vector3 normal = sideVec.Cross(axis);
			normal.Normalize();

			Matrix world(
				sideVec.x * kStickWidth, sideVec.y * kStickWidth, sideVec.z * kStickWidth, 0.0f,
				axis.x * len, axis.y * len, axis.z * len, 0.0f,
				normal.x, normal.y, normal.z, 0.0f,
				mid.x, mid.y, mid.z, 1.0f);

			Params prm = {};
			prm.color = Vector4(color.x, color.y, color.z, intensity * 1.6f);
			prm.mode = 0.0f;
			prm.baseWidthRatio = 1.0f;
			DrawQuad(world, prm);
		}

		//先の丸い光
		{
			float size = kTipSize * p.size * (0.7f + 0.3f * intensity);
			Vector3 normal = camPos - tip;
			normal.Normalize();
			Matrix world(
				camRight.x * size, camRight.y * size, camRight.z * size, 0.0f,
				camUp.x * size, camUp.y * size, camUp.z * size, 0.0f,
				normal.x, normal.y, normal.z, 0.0f,
				tip.x, tip.y, tip.z, 1.0f);

			Params prm = {};
			prm.color = Vector4(color.x, color.y, color.z, intensity);
			prm.mode = 1.0f;
			DrawQuad(world, prm);
		}
	}

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_NONE);
}

void OuterEdge::DrawQuad(const DirectX::SimpleMath::Matrix& world, const Params& params)
{
	Matrix w = world;
	Renderer::SetWorldMatrix(&w);
	m_Shader.UpdatePSConstantBuffer(&params, sizeof(params));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}
