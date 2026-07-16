#include "Note.h"
#include "Collision.h"
#include "Game.h"
#include "input.h"

using namespace std;


void Note::Init()
{
	// メッシュ読み込み
	StaticMesh staticmesh;

	// 3Dモデルデータ
	std::u8string modelFile = u8"assets/model/note/onpu.obj";
	// テクスチャディレクトリ
	std::string texDirectory = "assets/model/Siki";

	// Meshを読み込む
	std::string tmpStr1(reinterpret_cast<const char*>(modelFile.c_str()), modelFile.size());
	staticmesh.Load(tmpStr1, texDirectory);

	m_MeshRenderer.Init(staticmesh);

	// シェーダオブジェクト生成
	m_Shader.Create("shader/litTextureVS.hlsl", "shader/litTexturePS.hlsl");

	// サブセット情報取得
	m_subsets = staticmesh.GetSubsets();

	// テクスチャ情報取得
	m_Textures = staticmesh.GetTextures();

	// マテリアル情報取得
	vector<MATERIAL> materials = staticmesh.GetMaterials();

	// マテリアル数分ループ
	for (int i = 0; i < materials.size(); i++)
	{
		// マテリアルオブジェクト生成
		std::unique_ptr<Material> m = std::make_unique<Material>();

		// マテリアル情報をセット
		m->Create(materials[i]);

		// マテリアルオブジェクトを配列に追加
		m_Materials.push_back(std::move(m));
	}
	m_Scale.x = 0.02;
	m_Scale.y = 0.02;
	m_Scale.z = 0.02;
}

void Note::Update()
{
	if (!m_IsAlive) return;
	m_Timer += 1.0f / 60.0f;

	if (m_Timer >= m_LifeTime)
	{
		Kill();
	}

	m_Position += m_Direction * m_Speed;
}

void Note::Draw(Camera* cam)
{
	if (!m_IsAlive) return;
	// SRT情報作成
	Matrix r = Matrix::CreateFromYawPitchRoll(m_Rotation.y, m_Rotation.x, m_Rotation.z);
	Matrix t = Matrix::CreateTranslation(m_Position.x, m_Position.y, m_Position.z);
	Matrix s = Matrix::CreateScale(m_Scale.x, m_Scale.y, m_Scale.z);

	Matrix worldmtx;
	worldmtx = s * r * t;
	Renderer::SetWorldMatrix(&worldmtx); // GPUにセット

	m_Shader.SetGPU();

	// インデックスバッファ・頂点バッファをセット
	m_MeshRenderer.BeforeDraw();

	//マテリアル数分ループ 
	for (int i = 0; i < m_subsets.size(); i++)
	{
		// マテリアルをセット(サブセット情報の中にあるマテリアルインデックスを使用)
		m_Materials[m_subsets[i].MaterialIdx]->SetGPU();

		if (m_Materials[m_subsets[i].MaterialIdx]->isTextureEnable())
		{
			m_Textures[m_subsets[i].MaterialIdx]->SetGPU();
		}

		m_MeshRenderer.DrawSubset(
			m_subsets[i].IndexNum, // 描画するインデックス数
			m_subsets[i].IndexBase, // 最初のインデックスバッファの位置	
			m_subsets[i].VertexBase); // 頂点バッファの最初から使用
	}
}

void Note::Uninit()
{
}

void Note::SetData(const Vector3& pos, const Vector3& dir)
{
	m_Position = pos;
	m_Direction = dir;
}

void Note::RequestSpawn(const Vector3& pos, const Vector3& dir)
{
	//if(Oを押してから一定時間後（メトロノームが四回なるまで）に押されたOの数だけループ)
	Note* note = Game::GetInstance()->AddObject<Note>();
	note->SetData(pos, dir);
}
