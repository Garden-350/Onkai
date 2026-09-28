
#include "TitleScene.h"
#include "Game.h"
#include "Input.h"
#include "TitleCorridor.h"
#include "ScreenFade.h"

using namespace DirectX::SimpleMath;

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

	// 背景：たどり着けない回廊（一番奥に描くので最初に追加）
	m_Corridor = Game::GetInstance()->AddObject<TitleCorridor>();
	m_MySceneObjects.emplace_back(m_Corridor);

	// ↓タイトルロゴや「PRESS ENTER」はここ（回廊より後・フェードより前）に追加していく

	// 画面全体のフェード（一番手前に描くので最後に追加）
	m_Fade = Game::GetInstance()->AddObject<ScreenFade>();
	m_MySceneObjects.emplace_back(m_Fade);
	m_Fade->SetColor(Vector3(0.0f, 0.0f, 0.0f));
	m_Fade->SetAlpha(1.0f);	// 最初は真っ黒から
}

// 更新
void TitleScene::Update()
{
	if (!m_Corridor || !m_Fade) return;

	// エンターで出口へ（何度押しても最初の1回だけ効く）
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		m_Corridor->StartExit();
	}

	// 画面のフェード：最初は黒から明けて、出口に入ったら白く染まる
	float black = m_Corridor->GetIntroFade();
	float white = m_Corridor->GetExitWhite();
	if (white > black)
	{
		m_Fade->SetColor(Vector3(1.0f, 1.0f, 1.0f));
		m_Fade->SetAlpha(white);
	}
	else
	{
		m_Fade->SetColor(Vector3(0.0f, 0.0f, 0.0f));
		m_Fade->SetAlpha(black);
	}

	// 扉にたどり着いて真っ白になったら曲選択へ
	if (m_Corridor->IsExitFinished())
	{
		Game::GetInstance()->ChangeScene(SELECT);
		return;	// ChangeScene でこのシーンは消えているので、この後は何も触らない
	}
}


// 終了処理
void TitleScene::Uninit()
{
	for (auto& o : m_MySceneObjects) {
		Game::GetInstance()->DeleteObject(o);
	}
	m_MySceneObjects.clear();
	m_Corridor = nullptr;
	m_Fade = nullptr;
}
