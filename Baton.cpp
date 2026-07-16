#include "Baton.h"
#include "Collision.h"
#include "Game.h"
#include "input.h"
//ゲームオブジェクト
#include "Note.h"

using namespace std;
using namespace DirectX::SimpleMath;

void Baton::Init()
{
	// メッシュ読み込み
	StaticMesh staticmesh;

	// 3Dモデルデータ
	std::u8string modelFile = u8"assets/model/Siki/block.obj";
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
	m_Scale.x = 0.1;
	m_Scale.y = 0.1;
	m_Scale.z = 0.1;
}

void Baton::Update()
{
	float cam = Game::GetInstance()->GetCamera()->GetDirection();

	Vector3 forward;
	forward.x = sin(cam);
	forward.y = 0;
	forward.z = cos(cam);

	Vector3 slide;
	slide.x = cos(cam);
	slide.y = 0;
	slide.z = sin(cam);

	Vector3 moveDir = Vector3::Zero;

	if (Input::GetKeyPress(VK_W))
		moveDir -= forward;

	if (Input::GetKeyPress(VK_S))
		moveDir += forward;

	if (Input::GetKeyPress(VK_D))
		moveDir -= slide;

	if (Input::GetKeyPress(VK_A))
		moveDir += slide;

	if (moveDir.Length() > 0)
	{
		moveDir.Normalize();
		m_Position += moveDir * moveSpeed;
		m_Rotation.y = atan2(moveDir.x, moveDir.z);
	}

	if (Input::GetKeyTrigger(VK_O))
	{
		Vector3 spawnPos = m_Position + moveDir * 1.0f;

		Vector3 shotDir;
		shotDir.x = sin(m_Rotation.y);
		shotDir.y = 0;
		shotDir.z = cos(m_Rotation.y);

		//Noteにセット
		Game::GetInstance()->GetNote()->RequestSpawn(spawnPos, shotDir);
	}

}

void Baton::Draw(Camera* cam)
{
	//カメラを追従する
	cam->SetTarget(m_Position);	//カメラのターゲット更新
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

void Baton::Uninit()
{
}
