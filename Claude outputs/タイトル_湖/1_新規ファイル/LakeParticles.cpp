#include "LakeParticles.h"
#include "Camera.h"
#include <cmath>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr int kMaxLeaves = 48;
	constexpr int kFireflyCount = 36;

	float Saturate(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
}

void LakeParticles::Init(float shoreBaseZ)
{
	m_ShoreBaseZ = shoreBaseZ;

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

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/lakeParticlePS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(ParticleCB));

	m_Leaves.resize(kMaxLeaves);

	// ホタルは水際の草の上あたりに散らばらせておく
	std::uniform_real_distribution<float> U(0.0f, 1.0f);
	m_Fireflies.resize(kFireflyCount);
	for (auto& f : m_Fireflies)
	{
		float z = 140.0f + (shoreBaseZ + 80.0f - 140.0f) * U(m_Rng);
		float x = (U(m_Rng) * 2.0f - 1.0f) * (0.8f * z + 40.0f);
		f.anchor = Vector3(x, 15.0f + 70.0f * U(m_Rng), z);
		f.phase = U(m_Rng) * 100.0f;
		f.speed = 0.5f + 0.7f * U(m_Rng);
		f.blink = 0.6f + 0.8f * U(m_Rng);
	}
}

void LakeParticles::SpawnLeaf(const LakeParams& p)
{
	for (auto& l : m_Leaves)
	{
		if (l.alive) continue;
		std::uniform_real_distribution<float> U(0.0f, 1.0f);
		Vector3 wind(p.wind.x, 0.0f, p.wind.y);
		Vector3 side(-p.wind.y, 0.0f, p.wind.x);

		// 風上の、画面の外から飛んでくる
		float z = 120.0f + 420.0f * U(m_Rng);
		Vector3 center(0.0f, 0.0f, z);
		l.pos = center - wind * (0.9f * z + 150.0f) + side * ((U(m_Rng) * 2.0f - 1.0f) * 200.0f);
		l.pos.y = 20.0f + 170.0f * U(m_Rng);
		l.vel = wind * (120.0f + 100.0f * U(m_Rng));
		l.rot = U(m_Rng) * 6.283f;
		l.spin = (U(m_Rng) * 2.0f - 1.0f) * 6.0f;
		l.size = 5.0f + 4.0f * U(m_Rng);
		l.life = 0.0f;
		l.phase = U(m_Rng) * 10.0f;
		float g = U(m_Rng);
		l.albedo = (g < 0.6f) ? Vector3(0.10f, 0.20f, 0.04f)		// 緑の葉
			: (g < 0.85f) ? Vector3(0.35f, 0.25f, 0.05f)			// 黄色い葉
			: Vector3(0.30f, 0.10f, 0.04f);						// 赤っぽい葉
		l.alive = true;
		return;
	}
}

void LakeParticles::Update(float dt, const LakeParams& p)
{
	m_Time += dt;
	const float gust = p.wind.w;
	Vector3 wind(p.wind.x, 0.0f, p.wind.y);

	// ふだんは5秒に1枚くらい、突風のときはたくさん
	float rate = 0.2f + 14.0f * gust * gust;
	m_SpawnTimer += dt * rate;
	while (m_SpawnTimer >= 1.0f)
	{
		m_SpawnTimer -= 1.0f;
		SpawnLeaf(p);
	}

	for (auto& l : m_Leaves)
	{
		if (!l.alive) continue;
		l.life += dt;
		// 風に流される（突風のときは速い）。ひらひら上下しながら少しずつ落ちる
		Vector3 target = wind * (90.0f + 520.0f * gust);
		l.vel += (target - l.vel) * Saturate(dt * 1.5f);
		l.vel.y = sinf(m_Time * 3.0f + l.phase) * (25.0f + 60.0f * gust) - 18.0f + 40.0f * gust;
		l.pos += l.vel * dt;
		l.rot += l.spin * dt * (0.5f + gust);

		// 地面や水面に落ちた・画面の外へ行った・古くなったら消す
		float along = l.pos.x * wind.x + l.pos.z * wind.z;
		if (l.pos.y < 2.0f || along > 900.0f || l.life > 12.0f)
			l.alive = false;
	}
}

void LakeParticles::DrawQuad(const Vector3& center, float size, float rot, float aspect,
	const Vector3& camRight, const Vector3& camUp, const ParticleCB& cb)
{
	Vector3 r = (camRight * cosf(rot) + camUp * sinf(rot)) * size;
	Vector3 u = (camUp * cosf(rot) - camRight * sinf(rot)) * size * aspect;	// aspect=縦/横
	Vector3 n = r.Cross(u);
	if (n.LengthSquared() < 1e-6f) return;
	n.Normalize();
	Matrix world(
		r.x, r.y, r.z, 0.0f,
		u.x, u.y, u.z, 0.0f,
		n.x, n.y, n.z, 0.0f,
		center.x, center.y, center.z, 1.0f);
	Renderer::SetWorldMatrix(&world);
	m_Shader.UpdatePSConstantBuffer(&cb, sizeof(cb));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}

void LakeParticles::Draw(Camera* cam, const LakeParams& p)
{
	cam->SetCamera(0);
	const Vector3 camRight(p.camRight.x, p.camRight.y, p.camRight.z);
	const Vector3 camUp(p.camUp.x, p.camUp.y, p.camUp.z);
	const float exposure = p.camUp.w;
	const float night = p.sunColor.w;

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);
	Renderer::SetCullMode(CULL_NONE);
	Renderer::SetDepthEnable(false);	// 深度テストはする・書き込みはしない

	// ── 葉っぱ ──
	Renderer::SetBlendState(BS_ALPHABLEND);
	Vector3 light(p.sunColor.x * 0.6f + p.ambientCol.x, p.sunColor.y * 0.6f + p.ambientCol.y, p.sunColor.z * 0.6f + p.ambientCol.z);
	for (const auto& l : m_Leaves)
	{
		if (!l.alive) continue;
		ParticleCB cb;
		cb.color = Vector4(l.albedo.x * light.x, l.albedo.y * light.y, l.albedo.z * light.z,
			Saturate(l.life * 3.0f) * Saturate((12.0f - l.life) * 2.0f));
		cb.misc = Vector4(0.0f, exposure, 0.0f, 0.0f);
		DrawQuad(l.pos, l.size, l.rot, 1.6f, camRight, camUp, cb);	// 葉は縦長
	}

	// ── ホタル（夜だけ）──
	if (night > 0.05f)
	{
		Renderer::SetBlendState(BS_ADDITIVE);
		for (const auto& f : m_Fireflies)
		{
			float t = m_Time * f.speed + f.phase;
			Vector3 pos = f.anchor + Vector3(sinf(t * 0.7f) * 30.0f, sinf(t * 1.1f) * 10.0f, cosf(t * 0.5f) * 25.0f);
			float glow = powf(Saturate(sinf(m_Time * f.blink + f.phase)), 3.0f);	// ゆっくり明滅
			ParticleCB cb;
			cb.color = Vector4(0.6f * 3.0f, 1.0f * 3.0f, 0.3f * 3.0f, glow * night);
			cb.misc = Vector4(1.0f, exposure, 0.0f, 0.0f);
			DrawQuad(pos, 7.0f, 0.0f, 1.0f, camRight, camUp, cb);
		}
	}

	Renderer::SetDepthEnable(true);
	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetBlendState(BS_ALPHABLEND);
}
