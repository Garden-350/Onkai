#include "ResultScene.h"
#include "Game.h"
#include "Input.h"
#include "Texture2D.h"

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
	m_Result = Game::GetResultData();


	Renderer::SetDepthEnable(true);
	Texture2D* pt = Game::GetInstance()->AddObject<Texture2D>();
	pt->SetTexture("assets/texture/heigen.jpg");
	pt->SetPosition(0.0f, 0.0f, 485.0f);
	pt->SetRotation(0.0f, 0.0f, 0.0f);
	pt->SetScale(1280.0f, 720.0f, 0.0f);
	m_MySceneObjects.emplace_back(pt);

	SetUpDigitRow(m_PerfectDigits, m_Result.perfect, 400.0f, 300.0f);
	SetUpDigitRow(m_GreatDigits, m_Result.great, 400.0f, 240.0f);
	SetUpDigitRow(m_GoodDigits, m_Result.good, 400.0f, 180.0f);
	SetUpDigitRow(m_MissDigits, m_Result.miss, 400.0f, 120.0f);
	SetUpDigitRow(m_MaxComboDigits, m_Result.maxCombo, 400.0f, 60.0f);
}

// 更新
void ResultScene::Update()
{
	// エンターキーを押してタイトルへ
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		Game::GetInstance()->ChangeScene(TITLE);
	}
}


// 終了処理
void ResultScene::Uninit()
{
	// このシーンのオブジェクトを削除する
	for (auto& o : m_MySceneObjects) {
		Game::GetInstance()->DeleteObject(o);
	}
	m_MySceneObjects.clear();
}

void ResultScene::SetUpDigitRow(Texture2D** row, int value, float x, float y)
{
	if (value > 999) value = 999;
	if (value < 0) value = 0;
	int digits[3] = { value / 100, (value / 10) % 10,value % 10 };

	float digitW = 40.0f;	//桁ごとの横幅

	for (int i = 0; i < 3; i++)
	{
		Renderer::SetBlendState(BS_NONE);
		Texture2D* tex = Game::GetInstance()->AddObject<Texture2D>();
		tex->Init();
		tex->SetTexture("assets/texture/number_2.png");
		tex->SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
		tex->SetScale(80.0f, 80.0f, 1.0f);
		tex->SetPosition(x + digitW * i, y, 0.0f);

		row[i] = tex;
		m_MySceneObjects.emplace_back(tex);
	}
}
