
#include "TitleScene.h"
#include "Game.h"
#include "Input.h"
#include "Texture2D.h"
#include "Stage1Scene.h"
#include "Collision.h"

// コンストラクタ
TitleScene::TitleScene()
{
	Init();
}

// デストラクタ
TitleScene::~TitleScene()
{
	Uninit();
}

// 初期化
void TitleScene::Init()
{
	Renderer::SetDepthEnable(true);
	Texture2D* pt = Game::GetInstance()->AddObject<Texture2D>();
	pt->SetTexture("assets/texture/Loma.jpg");
	pt->SetPosition(0.0f, 0.0f, 485.0f);
	pt->SetRotation(0.0f, 0.0f, 0.0f);
	pt->SetScale(1280.0f, 720.0f, 0.0f);
	m_MySceneObjects.emplace_back(pt);

	Texture2D* en = Game::GetInstance()->AddObject<Texture2D>();
	en->SetTexture("assets/titleText/en.png");
	en->SetPosition(0.0f, 0.0f, 485.0f);
	en->SetRotation(0.0f, 0.0f, 0.0f);
	en->SetScale(300.0f, 300.0f, 0.0f);
	m_MySceneObjects.emplace_back(en);

	Texture2D* oto = Game::GetInstance()->AddObject<Texture2D>();
	oto->SetTexture("assets/titleText/oto.png");
	oto->SetPosition(-100.0f, 0.0f, 485.0f);
	oto->SetRotation(0.0f, 0.0f, 0.0f);
	oto->SetScale(200.0f, 200.0f, 0.0f);
	m_MySceneObjects.emplace_back(oto);

	Texture2D* nyo = Game::GetInstance()->AddObject<Texture2D>();
	nyo->SetTexture("assets/titleText/ennyo.png");
	nyo->SetPosition(120.0f, 0.0f, 485.0f);
	nyo->SetRotation(0.0f, 0.0f, 0.0f);
	nyo->SetScale(200.0f, 200.0f, 0.0f);
	m_MySceneObjects.emplace_back(nyo);

	kai_in = Game::GetInstance()->AddObject<Texture2D>();
	kai_in->SetTexture("assets/titleText/kai_in.png");
	kai_in->SetPosition(140.0f, 50.0f, 485.0f);
	kai_in->SetRotation(0.0f, 0.0f, m_kai_in_z);
	kai_in->SetScale(75.0f, 75.0f, 0.0f);
	m_MySceneObjects.emplace_back(kai_in);

	kai_out = Game::GetInstance()->AddObject<Texture2D>();
	kai_out->SetTexture("assets/titleText/kai_out.png");
	kai_out->SetPosition(140.0f, 50.0f, 485.0f);
	kai_out->SetRotation(0.0f, 0.0f, m_kai_out_z);
	kai_out->SetScale(140.0f, 140.0f, 0.0f);
	m_MySceneObjects.emplace_back(kai_out);
}

// 更新
void TitleScene::Update()
{
	m_kai_in_z += 0.3f;
	m_kai_out_z -= 0.3f;

	kai_in->SetRotation(0.0f, 0.0f, m_kai_in_z);
	kai_out->SetRotation(0.0f, 0.0f, m_kai_out_z);

	// エンターキーを押してタイトルへ
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		Game::GetInstance()->ChangeScene(SELECT);
	}
}


// 終了処理
void TitleScene::Uninit()
{
	for (auto& o : m_MySceneObjects) {
		Game::GetInstance()->DeleteObject(o);
	}
	m_MySceneObjects.clear();
}
