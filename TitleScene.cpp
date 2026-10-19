
#include "TitleScene.h"
#include "Game.h"
#include "Input.h"
#include "Texture2D.h"
#include "Stage1Scene.h"
#include "Collision.h"
#include "ScreenFade.h"

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
	//背景：湖のほとり
	//m_Lake = Game::GetInstance()->AddObject<TitleLake>();
	//m_MySceneObjects.emplace_back(m_Lake);
	//Texture2D* title = Game::GetInstance()->AddObject<Texture2D>();
	//title->SetTexture("assets/texture/saiba-.jpg");
	//title->SetPosition(0.0f, 0.0f, 10.0f);
	//title->SetScale(1920.0f, 1080.0f, 0.0f);
	//m_MySceneObjects.emplace_back(title);

	Texture2D* en = Game::GetInstance()->AddObject<Texture2D>();
	en->SetTexture("assets/titleText/en_w.png");
	en->SetPosition(0.0f, 0.0f, 485.0f);
	en->SetRotation(0.0f, 0.0f, 0.0f);
	en->SetScale(300.0f, 300.0f, 0.0f);
	m_MySceneObjects.emplace_back(en);
	m_Logo.emplace_back(en);

	Texture2D* oto = Game::GetInstance()->AddObject<Texture2D>();
	oto->SetTexture("assets/titleText/oto_w.png");
	oto->SetPosition(-100.0f, 0.0f, 0.0f);
	oto->SetRotation(0.0f, 0.0f, 0.0f);
	oto->SetScale(200.0f, 200.0f, 0.0f);
	m_MySceneObjects.emplace_back(oto);
	m_Logo.emplace_back(oto);

	Texture2D* nyo = Game::GetInstance()->AddObject<Texture2D>();
	nyo->SetTexture("assets/titleText/ennyo_w.png");
	nyo->SetPosition(120.0f, 0.0f, 0.0f);
	nyo->SetRotation(0.0f, 0.0f, 0.0f);
	nyo->SetScale(200.0f, 200.0f, 0.0f);
	m_MySceneObjects.emplace_back(nyo);
	m_Logo.emplace_back(nyo);

	kai_in = Game::GetInstance()->AddObject<Texture2D>();
	kai_in->SetTexture("assets/titleText/kai_in_w.png");
	kai_in->SetPosition(140.0f, 50.0f, 0.0f);
	kai_in->SetRotation(0.0f, 0.0f, m_kai_in_z);
	kai_in->SetScale(75.0f, 75.0f, 0.0f);
	m_MySceneObjects.emplace_back(kai_in);
	m_Logo.emplace_back(kai_in);

	kai_out = Game::GetInstance()->AddObject<Texture2D>();
	kai_out->SetTexture("assets/titleText/kai_out_w.png");
	kai_out->SetPosition(140.0f, 50.0f, 0.0f);
	kai_out->SetRotation(0.0f, 0.0f, m_kai_out_z);
	kai_out->SetScale(140.0f, 140.0f, 0.0f);
	m_MySceneObjects.emplace_back(kai_out);
	m_Logo.emplace_back(kai_out);

	//画面全体のフェード
	m_Fade = Game::GetInstance()->AddObject<ScreenFade>();
	m_MySceneObjects.emplace_back(m_Fade);
	m_Fade->SetColor(Vector3(0.0f, 0.0f, 0.0f));
	m_Fade->SetAlpha(1.0f);
}

// 更新
void TitleScene::Update()
{
	// 経過時間（Game::GetDeltaTime は 0 のままなので自分で測る）
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted) { m_LastClock = now; m_ClockStarted = true; }
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	if (dt > 0.1f) dt = 0.1f;   // 読み込み直後などで大きく飛ばないように

	// 廻の回転（秒基準にすると PC の速さで回転速度が変わらない）
	m_kai_in_z += 18.0f * dt;  // 今の「1フレーム 0.3」を 60fps で換算した値
	m_kai_out_z -= 18.0f * dt;
	kai_in->SetRotation(0.0f, 0.0f, m_kai_in_z);
	kai_out->SetRotation(0.0f, 0.0f, m_kai_out_z);

	if (!m_Leaving)
	{
		// 黒から 0.8 秒で明ける
		m_FadeAlpha -= dt / 0.8f;
		if (m_FadeAlpha < 0.0f) m_FadeAlpha = 0.0f;

		if (Input::GetKeyTrigger(VK_RETURN)) m_Leaving = true;
	}
	else
	{
		// 0.5 秒で暗転してから曲選択へ
		m_FadeAlpha += dt / 0.5f;
		if (m_FadeAlpha >= 1.0f)
		{
			Game::GetInstance()->ChangeScene(SELECT);
			return;   // このシーンはもう消えているので、この後は何も触らない
		}
	}
	m_Fade->SetAlpha(m_FadeAlpha);
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
