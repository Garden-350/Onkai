#include "Stage1Scene.h"
#include "Game.h"
#include "Input.h"
#include "ChartObject.h"
#include "SkyBox.h"
#include "Enemy.h"

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
	// オブジェクトを作成
	//ここでカメラ固定してます。必要なら変数で動くようにしてあげてね
	Game::GetInstance()->GetCamera()->SetFixedPose(
		Vector3(50.0f, -30.0f, -600.0f),   // カメラ位置：プレイヤーに近い高さ・少し手前
		Vector3(0.0f, 120.0f, 400.0f)     // 注視点：敵がいる高い位置・奥のほうへ
	);
	//譜面再生オブジェクトを生成してJSONをロード
	// たぶんうまくいってる
	auto* skybox = Game::GetInstance()->AddObject<SkyBox>();
	m_MySceneObjects.emplace_back(skybox);
	auto* player = Game::GetInstance()->AddObject<ChartObject>();
	player->LoadChart("0808");	//charts/chartname.jsonを読み込み
	m_MySceneObjects.emplace_back(player);
	auto* enemy = Game::GetInstance()->AddObject<Enemy>();
	m_MySceneObjects.emplace_back(enemy);
	player->SetEnemyRef(enemy);
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

