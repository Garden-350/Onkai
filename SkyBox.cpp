#include "SkyBox.h"
#include "Game.h"

void SkyBox::Init()
{
	//const char* skyTextures[6] = {
	//	"assets/texture/S.png",
	//	"assets/texture/D.png",
	//	"assets/texture/F.png",
	//	"assets/texture/J.png",
	//	"assets/texture/K.png",
	//	"assets/texture/L.png",
	//};
	//for (int i = 0; i < 6; i++)
	//{
	//	m_SkyBoxIndex[i].Init();
	//	m_SkyBoxIndex[i].SetTexture(skyTextures[i]);
	//	m_SkyBoxIndex[i].SetScale(1000.0f, 1000.0f, 1);	//仮決め
	//}

	////各面の向き
	//m_SkyBoxIndex[0].SetRotation(0, -90, 0);	//right
	//m_SkyBoxIndex[1].SetRotation(0, 90, 0);		//left
	//m_SkyBoxIndex[2].SetRotation(90, 0, 0);		//top
	//m_SkyBoxIndex[3].SetRotation(-90, 0, 0);	//bottom
	//m_SkyBoxIndex[4].SetRotation(0, 180, 0);	//front
	//m_SkyBoxIndex[5].SetRotation(0, 0, 0);		//back

	StaticMesh mesh;

	mesh.Load(
		//ここに3Dモデルを読みこみ 今仮表示あとで四角のやつ作ってね
		"assets/model/SkyBlock/SkyBox1.fbx",
		"assets/model/SkyBox"
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

void SkyBox::Update()
{
	m_Position = Game::GetInstance()->GetCamera()->GetPosition();
}

void SkyBox::Draw(Camera* cam)
{
	cam->SetCamera(0);

	Renderer::SetBlendState(BS_NONE);

	//深度を書き込まない
	Renderer::SetDepthEnable(false);

	//箱の内部を描画 一回これなしで読み込んでみてうまくいかんかったらrenderer内に記入
	//Renderer::SetCullMode(CULL_FRONT);

	m_Shader.SetGPU();

	m_MeshRenderer.BeforeDraw();

	Matrix s = Matrix::CreateScale(m_Size);
	Matrix t = Matrix::CreateTranslation(m_Position);

	Matrix world = s * t;

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

		//Renderer::SetCullMode(D3D10_CULL_BACK);
	}
	Renderer::SetDepthEnable(true);
}

void SkyBox::Uninit()
{
}
