#include "Enemy.h"
#include "Game.h"
#undef max

void Enemy::Init()
{
	StaticMesh mesh;

	mesh.Load(
		//ここに3Dモデルを読みこみ 今仮表示あとで四角のやつ作ってね
		"assets/model/Enemy/teki.fbx",
		"assets/model/Enemy"
	);

	m_MeshRenderer.Init(mesh);

	m_Shader.Create(
		"shader/litTextureVS.hlsl",
		"shader/enemyFlash.hlsl"
	);
	m_Shader.CreatePSConstantBuffer(sizeof(FlashParams));

	m_Subsets = mesh.GetSubsets();
	m_Textures = mesh.GetTextures();

	auto materials = mesh.GetMaterials();

	for (auto& m : materials)
	{
		auto material = std::make_unique<Material>();
		material->Create(m);
		m_Materials.emplace_back(std::move(material));
	}
}

void Enemy::Update()
{
	m_Rotation.y += 0.01f;
	if (m_HitTimer > 0.0f) m_HitTimer -= 1.0f / 60.0f;
	if (m_FlashTime > 0.0f) m_FlashTime = std::max(0.0f, m_FlashTime - 1.0f / 60.0f);
}

void Enemy::Draw(Camera* cam)
{
	cam->SetCamera(0);

	Renderer::SetBlendState(BS_NONE);

	//深度を書き込まない
	Renderer::SetDepthEnable(false);

	m_Shader.SetGPU();

	//今の光り具合をシェーダーへ送る
	FlashParams fp;
	fp.color = DirectX::SimpleMath::Vector4(m_FlashColor.x, m_FlashColor.y, m_FlashColor.z, CurrentFlash());
	m_Shader.UpdatePSConstantBuffer(&fp, sizeof(fp));

	m_MeshRenderer.BeforeDraw();

	Matrix s = Matrix::CreateScale(m_Size);
	//攻撃を受けた直後は左右に細かく揺らす
	float k = std::max(0.0f, m_HitTimer / 0.25f);
	Vector3 shake(sinf(m_HitTimer * 120.0f) * 12.0f * k, 0.0f, 0.0f);
	Matrix t = Matrix::CreateTranslation(m_Position + shake);
	Matrix x = Matrix::CreateRotationX(m_Rotation.x);
	Matrix y = Matrix::CreateRotationY(m_Rotation.y);
	Matrix z = Matrix::CreateRotationZ(m_Rotation.z);

	Matrix world = s * x * y * z * t;

	Renderer::SetWorldMatrix(&world);

	for (size_t i = 0; i < m_Subsets.size(); i++)
	{
		m_Materials[m_Subsets[i].MaterialIdx]->SetGPU();

		if (m_Materials[m_Subsets[i].MaterialIdx]->isTextureEnable())
		{
			m_Textures[m_Subsets[i].MaterialIdx]->SetGPU();
		}
		m_MeshRenderer.DrawSubset(
			m_Subsets[i].IndexNum,
			m_Subsets[i].IndexBase,
			m_Subsets[i].VertexBase
		);
	}
	Renderer::SetDepthEnable(true);
}

void Enemy::Uninit()
{
}

void Enemy::Flash(const DirectX::SimpleMath::Vector3& color, float strength, float duration)
{
	//今光っている方が強ければ、そちらを残す（弱い光で強い光を上書きしない）
	if (strength < CurrentFlash()) return;

	m_FlashColor = color;
	m_FlashStrength = strength;
	m_FlashDuration = std::max(0.01f, duration);
	m_FlashTime = m_FlashDuration;
}

//今の光り具合：光った瞬間が一番強く、2乗で素早く元に戻る
float Enemy::CurrentFlash() const
{
	if (m_FlashTime <= 0.0f) return 0.0f;
	float k = m_FlashTime / m_FlashDuration;	// 1->0
	return m_FlashStrength * k * k;
}
