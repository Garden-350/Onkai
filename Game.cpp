#include "Game.h"
#include "Renderer.h"
#include "Input.h"


Game* Game::m_Instance;


// コンストラクタ
Game::Game()
{
	m_Scene = nullptr;
}

// デストラクタ
Game::~Game()
{
	delete m_Scene;
	DeleteAllObject();
}

// 初期化
void Game::Init()
{
	//インスタンス作成
	m_Instance = new Game;
	// 描画初期化処理
	Renderer::Init();

	Input::Create();

	// カメラ初期化
	m_Instance->m_Camera.Init();

	//最初のシーンを読み込む
	m_Instance->m_Scene = new Stage1Scene;
}

// 更新
void Game::Update()
{
	//入力処理更新
	Input::Update();

	//シーン更新
	m_Instance->m_Scene->Update();

	// カメラ更新
	m_Instance->m_Camera.Update();

	
	for (auto& o : m_Instance -> m_Objects)
	{
		o->Update();
	}
	erase_if(m_Instance->m_Objects,
		[](const std::unique_ptr<Object>& o)
		{
			if (!o->IsAlive())
			{
				o->Uninit();
				return true;
			}
			return false;
		});

	for (auto& o : m_Instance->m_AddBuffer)
	{
		m_Instance->m_Objects.emplace_back(std::move(o));
	}
	m_Instance->m_AddBuffer.clear();
}

// 描画
void Game::Draw()
{
	// 描画前処理
	Renderer::DrawStart();

	
	for (auto& o : m_Instance->m_Objects)
	{
		o->Draw(&m_Instance->m_Camera);
	}

	// 描画後処理
	Renderer::DrawEnd();
}

// 終了処理
void Game::Uninit()
{
	// カメラ終了処理
	m_Instance->m_Camera.Uninit();

	
	for (auto& o : m_Instance->m_Objects)
	{
		o->Uninit();
	}
	//入力完了処理
	Input::Release();

	// 描画終了処理
	Renderer::Uninit();

	//インスタンス削除
	delete m_Instance;
}

//インスタンスを取得
Game* Game::GetInstance()
{
	return m_Instance;
}

//シーンを切り替える
void Game::ChangeScene(SceneName sName)
{
	//読み込み済みのシーンがあれば削除
	if (m_Instance->m_Scene != nullptr)
	{
		delete m_Instance->m_Scene;
		m_Instance->m_Scene = nullptr;
	}

	//カメラの角度をリセット
	m_Instance->m_Camera.Init();
	switch (sName)
	{
	case TITLE:
		m_Instance->m_Scene = new TitleScene;
		break;

	case STAGE1:
		m_Instance->m_Scene = new Stage1Scene;
		break;

	case RESULT:
		m_Instance->m_Scene = new ResultScene;
		break;
	}
}

void Game::DeleteObject(Object* pt)
{
	if (pt == NULL) return;

	pt->Uninit();

	//要素を削除
	erase_if(m_Instance->m_Objects,
		[pt](const std::unique_ptr<Object>& element) {
			return element.get() == pt;
		});
	m_Instance->m_Objects.shrink_to_fit();
}

void Game::DeleteAllObject()
{
	//オブジェクト終了処理
	for (auto& o : m_Instance->m_Objects)
	{
		o->Uninit();
	}

	m_Instance->m_Objects.clear();	//全て削除
	m_Instance->m_Objects.shrink_to_fit();
}
