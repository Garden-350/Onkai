
#include "TitleScene.h"
#include "Game.h"
#include "Input.h"
#include "Texture2D.h"
#include "TitleLake.h"
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

	// 背景：湖のほとり（一番奥に描くので最初に追加）
	m_Lake = Game::GetInstance()->AddObject<TitleLake>();
	m_MySceneObjects.emplace_back(m_Lake);

	// ── タイトルロゴ「音廻」（白い線の画像を使って、時間帯で色を付ける）──
	Texture2D* en = Game::GetInstance()->AddObject<Texture2D>();
	en->SetTexture("assets/titleText/en_w.png");
	en->SetPosition(0.0f, 0.0f, 485.0f);
	en->SetRotation(0.0f, 0.0f, 0.0f);
	en->SetScale(300.0f, 300.0f, 0.0f);
	m_MySceneObjects.emplace_back(en);
	m_Logo.emplace_back(en);

	Texture2D* oto = Game::GetInstance()->AddObject<Texture2D>();
	oto->SetTexture("assets/titleText/oto_w.png");
	oto->SetPosition(-100.0f, 0.0f, 485.0f);
	oto->SetRotation(0.0f, 0.0f, 0.0f);
	oto->SetScale(200.0f, 200.0f, 0.0f);
	m_MySceneObjects.emplace_back(oto);
	m_Logo.emplace_back(oto);

	Texture2D* nyo = Game::GetInstance()->AddObject<Texture2D>();
	nyo->SetTexture("assets/titleText/ennyo_w.png");
	nyo->SetPosition(120.0f, 0.0f, 485.0f);
	nyo->SetRotation(0.0f, 0.0f, 0.0f);
	nyo->SetScale(200.0f, 200.0f, 0.0f);
	m_MySceneObjects.emplace_back(nyo);
	m_Logo.emplace_back(nyo);

	kai_in = Game::GetInstance()->AddObject<Texture2D>();
	kai_in->SetTexture("assets/titleText/kai_in_w.png");
	kai_in->SetPosition(140.0f, 50.0f, 485.0f);
	kai_in->SetRotation(0.0f, 0.0f, m_kai_in_z);
	kai_in->SetScale(75.0f, 75.0f, 0.0f);
	m_MySceneObjects.emplace_back(kai_in);
	m_Logo.emplace_back(kai_in);

	kai_out = Game::GetInstance()->AddObject<Texture2D>();
	kai_out->SetTexture("assets/titleText/kai_out_w.png");
	kai_out->SetPosition(140.0f, 50.0f, 485.0f);
	kai_out->SetRotation(0.0f, 0.0f, m_kai_out_z);
	kai_out->SetScale(140.0f, 140.0f, 0.0f);
	m_MySceneObjects.emplace_back(kai_out);
	m_Logo.emplace_back(kai_out);

	// 画面全体のフェード（一番手前に描くので最後に追加）
	m_Fade = Game::GetInstance()->AddObject<ScreenFade>();
	m_MySceneObjects.emplace_back(m_Fade);
	m_Fade->SetColor(Vector3(0.0f, 0.0f, 0.0f));
	m_Fade->SetAlpha(1.0f);	// 最初は真っ黒から
}

// 更新
void TitleScene::Update()
{
	if (!m_Lake || !m_Fade) return;

	// 「廻」の輪を逆向きに回す（SetRotation は度で指定）
	m_kai_in_z += 0.3f;
	m_kai_out_z -= 0.3f;
	kai_in->SetRotation(0.0f, 0.0f, m_kai_in_z);
	kai_out->SetRotation(0.0f, 0.0f, m_kai_out_z);

	// ロゴの色：昼は濃い紺、夜は白く光る。飛び込みが始まったら消えていく
	float night = m_Lake->GetNightAmount();
	Vector3 dayCol(0.05f, 0.06f, 0.10f);
	Vector3 nightCol(0.92f, 0.95f, 1.0f);
	Vector3 c = dayCol + (nightCol - dayCol) * night;
	float dive = m_Lake->GetDiveProgress();
	float alpha = 1.0f - (dive < 0.35f ? dive / 0.35f : 1.0f);
	for (auto* logo : m_Logo) logo->SetColor(c.x, c.y, c.z, alpha);

	// エンターで湖へ飛び込む（何度押しても最初の1回だけ効く）
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		m_Lake->StartDive();
	}

	// 画面のフェード：最初は黒から明けて、水に入った瞬間に水の色で覆う
	float intro = m_Lake->GetIntroFade();
	float splash = m_Lake->GetDiveFade();
	if (splash > intro)
	{
		m_Fade->SetColor(m_Lake->GetDiveFadeColor());
		m_Fade->SetAlpha(splash);
	}
	else
	{
		m_Fade->SetColor(Vector3(0.0f, 0.0f, 0.0f));
		m_Fade->SetAlpha(intro);
	}

	// 水に入りきったら曲選択へ
	if (m_Lake->IsDiveFinished())
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
	m_Logo.clear();
	m_Lake = nullptr;
	m_Fade = nullptr;
	kai_in = nullptr;
	kai_out = nullptr;
}
