#include "ResultScene.h"
#include "Game.h"
#include "Input.h"
#include "Texture2D.h"
#include "sound.h"
#include <filesystem>

namespace fs = std::filesystem;

// コンストラクタ
ResultScene::ResultScene()
{
	Init();
}

// デストラクタ
ResultScene::~ResultScene()
{
	Uninit();
}

// 初期化
void ResultScene::Init()
{
	sound.Init();

	sound.Play(SOUND_RESULT_1);

	Renderer::SetDepthEnable(true);
	Texture2D* pt = Game::GetInstance()->AddObject<Texture2D>();
	pt->SetTexture("assets/texture/stage_backdrop.png");
	pt->SetPosition(0.0f, 0.0f, 485.0f);
	pt->SetRotation(0.0f, 0.0f, 0.0f);
	pt->SetScale(1280.0f, 720.0f, 0.0f);
	m_MySceneObjects.emplace_back(pt);
}

// 更新
void ResultScene::Update()
{
	if (!m_Initialized)
	{
		m_Result = Game::GetResultData();
		SetUpDigitRow(m_PerfectDigits, m_Result.perfect, 400.0f, 260.0f);
		SetUpDigitRow(m_GreatDigits, m_Result.great, 400.0f, 160.0f);
		SetUpDigitRow(m_GoodDigits, m_Result.good, 400.0f, 60.0f);
		SetUpDigitRow(m_MissDigits, m_Result.miss, 400.0f, -60.0f);
		SetUpDigitRow(m_MaxComboDigits, m_Result.maxCombo, -400.0f, -30.0f);

		//SelectSceneで選んだジャケットを表示
		const std::string& name = m_Result.chartName;
		const char* exts[] = { ".png", ".jpeg", ".jpg" };
		Texture2D* jacket = Game::GetInstance()->AddObject<Texture2D>();
		bool loaded = false;
		for (const char* ext : exts)
		{
			std::string path = "assets/jacket/" + name + ext;
			if (std::filesystem::exists(path))
			{
				jacket->SetTexture(path.c_str());
				loaded = true;
				break;
			}
		}
		if (!loaded)
		{
			jacket->SetTexture("assets/jacket/default.png");
		}
		jacket->SetPosition(-400.0f, 230.0f, 0.0f);
		jacket->SetScale(200.0f, 200.0f, 0.0f);
		m_MySceneObjects.emplace_back(jacket);
		//================================================================================

		Texture2D* combo = Game::GetInstance()->AddObject<Texture2D>();
		combo->SetTexture("assets/texture/COMBO.png");
		combo->SetPosition(-400.0f, 50.0f, 0.0f);
		combo->SetRotation(0.0f, 0.0f, 0.0f);
		combo->SetScale(200.0f, 60.0f, 0.0f);
		m_MySceneObjects.emplace_back(combo);

		Texture2D* pef = Game::GetInstance()->AddObject<Texture2D>();
		pef->SetTexture("assets/texture/PERFECT.png");
		pef->SetPosition(200.0f, 260.0f, 0.0f);
		pef->SetRotation(0.0f, 0.0f, 0.0f);
		pef->SetScale(200.0f,60.0f, 0.0f);
		m_MySceneObjects.emplace_back(pef);

		Texture2D* gre = Game::GetInstance()->AddObject<Texture2D>();
		gre->SetTexture("assets/texture/GREAT.png");
		gre->SetPosition(200.0f, 160.0f, 0.0f);
		gre->SetRotation(0.0f, 0.0f, 0.0f);
		gre->SetScale(200.0f, 60.0f, 0.0f);
		m_MySceneObjects.emplace_back(gre);

		Texture2D* good = Game::GetInstance()->AddObject<Texture2D>();
		good->SetTexture("assets/texture/GOOD.png");
		good->SetPosition(200.0f, 60.0f, 0.0f);
		good->SetRotation(0.0f, 0.0f, 0.0f);
		good->SetScale(200.0f, 60.0f, 0.0f);
		m_MySceneObjects.emplace_back(good);

		Texture2D* mis = Game::GetInstance()->AddObject<Texture2D>();
		mis->SetTexture("assets/texture/MISS.png");
		mis->SetPosition(200.0f, -60.0f, 0.0f);
		mis->SetRotation(0.0f, 0.0f, 0.0f);
		mis->SetScale(200.0f, 60.0f, 0.0f);
		m_MySceneObjects.emplace_back(mis);

		if (m_Result.isFullCombo)
		{
			Texture2D* full = Game::GetInstance()->AddObject<Texture2D>();
			full->SetTexture("assets/texture/FullCpmbo.png");
			full->SetPosition(0.0f, 0.0f, 0.0f);
			full->SetRotation(0.0f, 0.0f, 0.0f);
			full->SetScale(360.0f, 260.0f, 0.0f);
			m_MySceneObjects.emplace_back(full);
		}

		Texture2D* ret = Game::GetInstance()->AddObject<Texture2D>();
		ret->SetTexture("assets/texture/retry_white.png");
		ret->SetPosition(-580.0f, 300.0f, 0.0f);
		ret->SetScale(100.0f, 100.0f, 0.0f);
		m_MySceneObjects.emplace_back(ret);

		m_Initialized = true;
	}

	// エンターキーを押してタイトルへ
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		Game::GetInstance()->ChangeScene(SELECT);
	}
}


// 終了処理
void ResultScene::Uninit()
{
	sound.Uninit();
	// このシーンのオブジェクトを削除する
	for (auto& o : m_MySceneObjects) {
		Game::GetInstance()->DeleteObject(o);
	}
	m_MySceneObjects.clear();
}

void ResultScene::SetUpDigitRow(Texture2D** row, int value, float x, float y)
{
	if (value > 9999) value = 9999;
	if (value < 0) value = 0;
	int digits[4] = { value / 1000, (value / 100) % 10, (value / 10) % 10,value % 10 };

	float digitW = 60.0f;	//桁ごとの横幅

	for (int i = 0; i < 4; i++)
	{
		Renderer::SetBlendState(BS_ALPHABLEND);
		Texture2D* tex = Game::GetInstance()->AddObject<Texture2D>();
		tex->SetTexture("assets/texture/allnumber_new.png");
		tex->SetUV((digits[i] + 1), 1, 10, 1);
		tex->SetScale(80.0f, 80.0f, 1.0f);
		tex->SetPosition(x + digitW * i, y, -10.0f);

		m_MySceneObjects.emplace_back(tex);
		row[i] = tex;
	}
}
