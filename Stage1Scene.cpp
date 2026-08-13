#include "Stage1Scene.h"
#include "Game.h"
#include "Input.h"
#include "ChartObject.h"
#include "SkyBox.h"
#include "Enemy.h"
#include "Player.h"

using namespace DirectX::SimpleMath;



// コンストラクタ
Stage1Scene::Stage1Scene()
{
	Init();
}

// デストラクタ
Stage1Scene::~Stage1Scene()
{
	Uninit();
}



// 初期化
void Stage1Scene::Init()
{
	// 横からの斜め見下ろし視点：
	// プレイヤーの斜め後方・少し上から、奥の敵方向を見下ろす
	Game::GetInstance()->GetCamera()->SetFixedPose(
		Vector3(0.0f, 300.0f, -850.0f),   // カメラ位置：プレイヤーのやや上・後方
		Vector3(0.0f, 100.0f, -300.0f)      // 注視点：奥(敵方向)の少し高い位置
	);

	//auto* skybox = Game::GetInstance()->AddObject<SkyBox>();
	//m_MySceneObjects.emplace_back(skybox);
	auto* chart = Game::GetInstance()->AddObject<ChartObject>();
	chart->LoadChart("taptest");
	m_MySceneObjects.emplace_back(chart);
	auto* enemy = Game::GetInstance()->AddObject<Enemy>();
	m_MySceneObjects.emplace_back(enemy);
	auto* player = Game::GetInstance()->AddObject<Player>();
	m_MySceneObjects.emplace_back(player);
	chart->SetEnemyRef(enemy);
	chart->SetPlayerRef(player);
}



//更新
void Stage1Scene::Update()
{
	// エンターキーを押してタイトルへ
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		Game::GetInstance()->ChangeScene(RESULT);
	}
}

// 終了処理
void Stage1Scene::Uninit()
{
	// このシーンのオブジェクトを削除する
	for (auto& o : m_MySceneObjects) {
		Game::GetInstance()->DeleteObject(o);
	}
	m_MySceneObjects.clear();
}

