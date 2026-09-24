#include "SpectrumRing.h"
#include "Camera.h"
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	constexpr float kPi = 3.14159265f;
	constexpr float kDegToRad = kPi / 180.0f;

	// なめらかさ（見た目の調整用）
	constexpr float kAttackSpeed = 25.0f;	// 音が大きくなったときに追いつく速さ（大きいほど即座に伸びる）
	constexpr float kFallSpeed = 1.6f;		// 音が小さくなったときに縮む速さ（1秒あたり）

	// 色：真上(低音)→真下(高音)へグラデーション
	const Vector3 kLowColor(0.25f, 0.85f, 1.0f);	// 水色
	const Vector3 kHighColor(1.0f, 0.35f, 0.85f);	// ピンク
	const Vector3 kRingColor(0.55f, 0.8f, 1.0f);	// 土台の円
}

void SpectrumRing::Init()
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
	verts[0].uv = Vector2(0.0f, 0.0f);	// uv.y=0 がバーの先端側
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);	// uv.y=1 がバーの根本側
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/spectrumBarPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(SpectrumParams));
}

void SpectrumRing::SetLevels(const std::vector<float>& levels, float dt)
{
	if (m_Levels.size() != levels.size())
		m_Levels.assign(levels.size(), 0.0f);

	float attack = dt * kAttackSpeed;
	if (attack > 1.0f) attack = 1.0f;

	for (size_t i = 0; i < levels.size(); i++)
	{
		float target = levels[i];
		float cur = m_Levels[i];
		if (target > cur)
		{
			cur += (target - cur) * attack;		// 伸びるときは素早く
		}
		else
		{
			cur -= kFallSpeed * dt;				// 縮むときはゆっくり落ちる
			if (cur < target) cur = target;
		}
		m_Levels[i] = cur;
	}

	// 低音（下から1/8の帯域）の平均を「鼓動」用に取っておく
	size_t bassCount = levels.size() / 8;
	if (bassCount < 1) bassCount = 1;
	float sum = 0.0f;
	for (size_t i = 0; i < bassCount && i < m_Levels.size(); i++) sum += m_Levels[i];
	m_Bass = m_Levels.empty() ? 0.0f : sum / (float)bassCount;

	m_Spin += m_SpinSpeed * dt;
	if (m_Spin > 360.0f) m_Spin -= 360.0f;
}

void SpectrumRing::DrawQuad(const Matrix& world, const SpectrumParams& params)
{
	Matrix w = world;
	Renderer::SetWorldMatrix(&w);
	m_Shader.UpdatePSConstantBuffer(&params, sizeof(params));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}

void SpectrumRing::Draw(Camera* cam)
{
	if (m_Levels.empty()) return;

	cam->SetCamera(1);	// Texture2Dと同じ2D座標系

	Renderer::SetBlendState(BS_ADDITIVE);
	Renderer::SetDepthEnable(false);	// 深度テストはする（ジャケットの裏に隠れる）が、深度は書き込まない
	Renderer::SetCullMode(CULL_NONE);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);	// Texture2DがいじったUV行列を元に戻す

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// ── 背景を少し暗くする円（明るい背景画像の上でもバーが見えるように）──
	if (m_BackingAlpha > 0.0f)
	{
		Renderer::SetBlendState(BS_ALPHABLEND);
		float size = (m_Radius + m_MaxLength) * 2.0f + 40.0f;
		Matrix world = Matrix::CreateScale(size, size, 1.0f) * Matrix::CreateTranslation(m_Center + Vector3(0.0f, 0.0f, 1.0f));

		SpectrumParams p = {};
		p.color = Vector4(0.02f, 0.02f, 0.06f, m_BackingAlpha);
		p.mode = 2.0f;
		DrawQuad(world, p);
		Renderer::SetBlendState(BS_ADDITIVE);
	}

	// ── 土台の円（低音に合わせて少し明るくなる）──
	{
		float size = m_Radius * 2.0f + 8.0f;
		Matrix world = Matrix::CreateScale(size, size, 1.0f) * Matrix::CreateTranslation(m_Center);

		SpectrumParams p = {};
		p.color = Vector4(kRingColor.x, kRingColor.y, kRingColor.z, 0.25f + 0.55f * m_Bass);
		p.mode = 1.0f;
		p.ringWidth = 0.012f;
		DrawQuad(world, p);
	}

	// ── バー ──
	const int bandCount = (int)m_Levels.size();
	for (int k = 0; k < bandCount * 2; k++)
	{
		const bool rightSide = (k < bandCount);
		const int band = rightSide ? k : k - bandCount;
		const float frac = (band + 0.5f) / (float)bandCount;	// 0=低音(真上) 〜 1=高音(真下)

		// 真上(90度)から、右半分は時計回り・左半分は反時計回りに並べる
		float angleDeg = rightSide ? (90.0f - frac * 180.0f) : (90.0f + frac * 180.0f);
		angleDeg += m_Spin;
		const float angle = angleDeg * kDegToRad;

		const float level = m_Levels[band];
		const float length = m_MinLength + level * m_MaxLength;

		Vector3 dir(cosf(angle), sinf(angle), 0.0f);
		Vector3 center = m_Center + dir * (m_Radius + length * 0.5f);

		// ローカルのY軸(上向き)が dir を向くように回す
		Matrix world =
			Matrix::CreateScale(m_BarWidth, length, 1.0f) *
			Matrix::CreateRotationZ(angle - kPi * 0.5f) *
			Matrix::CreateTranslation(center);

		Vector3 col = kLowColor + (kHighColor - kLowColor) * frac;

		SpectrumParams p = {};
		p.color = Vector4(col.x, col.y, col.z, 0.35f + 0.65f * level);
		p.mode = 0.0f;
		DrawQuad(world, p);
	}

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_ALPHABLEND);
}
