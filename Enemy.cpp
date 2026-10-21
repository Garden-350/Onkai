#include "Enemy.h"
#include "Game.h"
#undef max

void Enemy::Init()
{
	StaticMesh mesh;

	mesh.Load(
		//‚±‚±‚É3Dƒ‚ƒfƒ‹‚ğ“Ç‚İ‚±‚İ ¡‰¼•\¦‚ ‚Æ‚ÅlŠp‚Ì‚â‚Âì‚Á‚Ä‚Ë
		"assets/model/Enemy/teki.fbx",
		"assets/model/Enemy"
	);

	m_MeshRenderer.Init(mesh);

	m_Shader.Create(
		"shader/litTextureVS.hlsl",
		"shader/litTexturePS.hlsl"
	);

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
}

void Enemy::Draw(Camera* cam)
{
	cam->SetCamera(0);

	Renderer::SetBlendState(BS_NONE);

	//[“x‚ğ‘‚«‚Ü‚È‚¢
	Renderer::SetDepthEnable(false);

	m_Shader.SetGPU();

	m_MeshRenderer.BeforeDraw();

	Matrix s = Matrix::CreateScale(m_Size);
	//UŒ‚‚ğó‚¯‚½’¼Œã‚Í¶‰E‚É×‚©‚­—h‚ç‚·
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
