#include "Game.h"
#include "Renderer.h"
#include "Input.h"
#include "main.h"
#include "ChartObject.h"


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

	//ポストエフェクト初期か
	m_Instance->m_PostFlash.Init(SCREEN_WIDTH, SCREEN_HEIGHT);

	//最初のシーンを読み込む
	m_Instance->m_Scene = new TitleScene;
}

// 更新
void Game::Update()
{
	//入力処理更新
	Input::Update();

	//ポーズ切替
	if (Input::GetKeyTrigger(VK_SPACE) && dynamic_cast<Stage1Scene*>(m_Instance->m_Scene))
	{
		m_Instance->m_Paused = !m_Instance->m_Paused;

		if (m_Instance->m_Paused)
		{
			//ポーズ開始時刻を記録
			m_Instance->m_PauseStartClock = std::chrono::steady_clock::now();
			//ポーズ中にループ音が鳴り続けないように止める
			for (auto& o : m_Instance->m_Objects)
			{
				if (auto chart = dynamic_cast<ChartObject*>(o.get()))
					chart->StopLongSounds();
			}
		}
		else
		{
			//ポーズしていた分譜面の内部時計を進めてずれ解消
			auto pauseDuration = std::chrono::steady_clock::now() - m_Instance->m_PauseStartClock;
			for (auto& o : m_Instance->m_Objects)
			{
				if (auto chart = dynamic_cast<ChartObject*>(o.get()))
				{
					chart->AddPauseOffset(pauseDuration);
				}
			}
		}
	}
	//カメラ更新
	m_Instance->m_Camera.Update();

	if (m_Instance->m_Paused)
	{
		//ポーズ中にRでリトライ
		if (Input::GetKeyTrigger(VK_R) && dynamic_cast<Stage1Scene*>(m_Instance->m_Scene))
		{
			m_Instance->m_Paused = false;
			m_Instance->ChangeScene(STAGE1);
		}
		else
		{
			return;
		}
	}
	if (Input::GetKeyTrigger(VK_R) && dynamic_cast<ResultScene*>(m_Instance->m_Scene))
	{
		m_Instance->ChangeScene(STAGE1);
	}
	//シーン更新
	m_Instance->m_Scene->Update();

	for (auto& o : m_Instance->m_Objects)
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
	//Renderer::DrawStart();
	m_Instance->m_PostFlash.BeginScene();
	
	Camera* cam = &m_Instance->m_Camera;

	//1回目に背景描画　SetWorldOffsetの影響を受ける
	cam->SetLayer(Camera::Layer::World);
	for (auto& o : m_Instance->m_Objects)
		if (o->GetLayer() == Camera::Layer::World)o->Draw(cam);

	//背景の奥行を消す　譜面は背景より手前
	Renderer::ClearCurrentDepth();

	//2回目　譜面描画
	cam->SetLayer(Camera::Layer::Playfield);
	for (auto& o : m_Instance->m_Objects)
		if (o->GetLayer() == Camera::Layer::Playfield)o->Draw(cam);

	float flashAlpha = 0.0f;
	for (auto& o : m_Instance->m_Objects)
	{
		if (auto chart = dynamic_cast<ChartObject*>(o.get()))
		{
			flashAlpha = chart->GetFlashAlpha();
			break;
		}
	}
	m_Instance->m_PostFlash.EndSceneAndApply(flashAlpha);
	//new
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

	case SELECT:
		m_Instance->m_Scene = new SelectScene;
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
