#include "Stage1Scene.h"
#include "Game.h"
#include "Input.h"
#include "ChartObject.h"
#include "SkyBox.h"

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
		Vector3(0.0f, 50.0f, -600.0f),	//カメラポス
		Vector3(0.0f, -55.0f, -200.0f)	//注視点
	);
	//譜面再生オブジェクトを生成してJSONをロード
	// たぶんうまくいってる
	auto* skybox = Game::GetInstance()->AddObject<SkyBox>();
	m_MySceneObjects.emplace_back(skybox);
	auto* player = Game::GetInstance()->AddObject<ChartObject>();
	player->LoadChart("bezye2");	//charts/chartname.jsonを読み込み
	m_MySceneObjects.emplace_back(player);
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

