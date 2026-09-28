#include "LakeGrass.h"
#include "LakeParams.h"
#include "Camera.h"
#include <random>
#include <cmath>

using namespace DirectX::SimpleMath;

void LakeGrass::AddBlade(std::vector<VERTEX_3D>& verts, std::vector<unsigned int>& idx,
	const Vector3& base, float yaw, float leanYaw, float lean,
	float width, float height, const Vector3& color, float rnd)
{
	const unsigned int start = (unsigned int)verts.size();
	const Vector3 side(cosf(yaw), 0.0f, -sinf(yaw));		// 葉の幅の向き
	const Vector3 normal(sinf(yaw), 0.0f, cosf(yaw));		// 葉の面の向き
	const Vector3 leanDir(sinf(leanYaw), 0.0f, cosf(leanYaw));
	const float levels[3] = { 0.0f, 0.38f, 0.72f };

	for (int i = 0; i < 3; i++)
	{
		float h = levels[i];
		Vector3 center = base + Vector3(0.0f, height * h, 0.0f) + leanDir * (lean * height * h * h);
		float w = width * (1.0f - h * 0.75f) * 0.5f;		// 先ほど細くなる
		for (int s = 0; s < 2; s++)
		{
			VERTEX_3D v{};
			v.position = center + side * (s == 0 ? -w : w);
			v.normal = normal;
			v.color = Color(color.x, color.y, color.z, rnd);
			v.uv = Vector2(h, height);
			verts.push_back(v);
		}
	}
	// 先端
	VERTEX_3D tip{};
	tip.position = base + Vector3(0.0f, height, 0.0f) + leanDir * (lean * height);
	tip.normal = normal;
	tip.color = Color(color.x, color.y, color.z, rnd);
	tip.uv = Vector2(1.0f, height);
	verts.push_back(tip);

	for (int i = 0; i < 2; i++)
	{
		unsigned int a = start + i * 2, b = a + 1, c = a + 2, d = a + 3;
		idx.insert(idx.end(), { a, b, c, b, d, c });
	}
	idx.insert(idx.end(), { start + 4, start + 5, start + 6 });
}

void LakeGrass::Init(float shoreBaseZ)
{
	std::mt19937 rng(20260928);	// 毎回同じ草原になるように種を固定
	std::uniform_real_distribution<float> U(0.0f, 1.0f);

	std::vector<VERTEX_3D> verts;
	std::vector<unsigned int> idx;
	verts.reserve((m_GrassCount + m_ReedCount) * 7);
	idx.reserve((m_GrassCount + m_ReedCount) * 15);

	// ── 草：カメラから見える範囲（奥ほど広い台形）に敷き詰める ──
	const float maxZ = shoreBaseZ + 60.0f;
	int placed = 0;
	while (placed < m_GrassCount)
	{
		float z = m_NearZ + (maxZ - m_NearZ) * U(rng);
		float halfW = 0.85f * z + 90.0f;
		float x = (U(rng) * 2.0f - 1.0f) * (0.85f * maxZ + 90.0f);
		if (fabsf(x) > halfW) continue;
		if (z > LakeShape::ShoreZ(x, shoreBaseZ) - 3.0f) continue;	// 水の上には生やさない

		float patch = 0.5f + 0.5f * sinf(x * 0.021f + 1.7f) * sinf(z * 0.017f + 0.3f);	// 背の高い所・低い所のむら
		float height = (8.0f + 18.0f * U(rng)) * (0.7f + 0.8f * patch);
		float width = 2.0f + 1.8f * U(rng);
		Vector3 col;
		if (U(rng) < 0.12f)
			col = Vector3(0.16f, 0.14f, 0.06f) * (0.8f + 0.4f * U(rng));				// 枯れ草
		else
			col = Vector3(0.05f + 0.05f * U(rng), 0.11f + 0.06f * U(rng), 0.025f + 0.02f * U(rng));
		AddBlade(verts, idx, Vector3(x, LakeShape::kBankY, z), U(rng) * 6.283f, U(rng) * 6.283f,
			0.15f + 0.35f * U(rng), width, height, col, U(rng));
		placed++;
	}

	// ── 葦：水際の左右にかたまって生える（湖の真ん中は見えるように空けておく。一部は水の中から） ──
	const int clusters = 12;
	for (int c = 0; c < clusters; c++)
	{
		float sideSign = (c % 2 == 0) ? -1.0f : 1.0f;
		float cx = sideSign * (0.35f + 0.65f * U(rng)) * (0.85f * shoreBaseZ + 60.0f);
		float cz = LakeShape::ShoreZ(cx, shoreBaseZ) + (U(rng) - 0.6f) * 50.0f;
		int n = m_ReedCount / clusters;
		for (int i = 0; i < n; i++)
		{
			float x = cx + (U(rng) + U(rng) - 1.0f) * 45.0f;
			float z = cz + (U(rng) + U(rng) - 1.0f) * 25.0f;
			float baseY = (z > LakeShape::ShoreZ(x, shoreBaseZ)) ? 0.0f : LakeShape::kBankY;	// 水の中なら水面から
			float height = 55.0f + 55.0f * U(rng);
			Vector3 col(0.10f + 0.05f * U(rng), 0.11f + 0.04f * U(rng), 0.045f + 0.02f * U(rng));
			AddBlade(verts, idx, Vector3(x, baseY, z), U(rng) * 6.283f, U(rng) * 6.283f,
				0.08f + 0.15f * U(rng), 1.4f + 0.8f * U(rng), height, col, U(rng));
		}
	}

	m_VertexBuffer.Create(verts);
	m_IndexBuffer.Create(idx);
	m_IndexCount = (unsigned int)idx.size();

	m_Shader.Create("shader/lakeGrassVS.hlsl", "shader/lakeGrassPS.hlsl");
}

void LakeGrass::Draw(Camera* cam, ID3D11Buffer* lakeCB)
{
	if (m_IndexCount == 0) return;
	auto* ctx = Renderer::GetDeviceContext();

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_NONE);
	Renderer::SetDepthEnable(true);
	Renderer::SetCullMode(CULL_NONE);	// 葉は両面

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	ctx->VSSetConstantBuffers(6, 1, &lakeCB);	// 風でなびかせるのに頂点シェーダーでも使う
	ctx->PSSetConstantBuffers(6, 1, &lakeCB);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->DrawIndexed(m_IndexCount, 0, 0);

	Renderer::SetCullMode(CULL_BACK);
}
