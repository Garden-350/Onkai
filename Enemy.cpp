#include "Enemy.h"
#include "Game.h"

void Enemy::Init()
{
	StaticMesh mesh;

	mesh.Load(
		//ここに3Dモデルを読みこみ 今仮表示あとで四角のやつ作ってね
		"assets/model/Enemy/enemy.fbx",
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
	m_Rotation.y += 0.1f;
}

void Enemy::Draw(Camera* cam)
{
	cam->SetCamera(0);

	Renderer::SetBlendState(BS_NONE);

	//深度を書き込まない
	Renderer::SetDepthEnable(false);

	m_Shader.SetGPU();

	m_MeshRenderer.BeforeDraw();

	Matrix s = Matrix::CreateScale(m_Size);
	Matrix t = Matrix::CreateTranslation(m_Position);
	Matrix r = Matrix::CreateRotationY(m_Rotation.y);

	Matrix world = s * r * t;

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
