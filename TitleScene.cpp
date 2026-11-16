
#include "TitleScene.h"
#include "Game.h"
#include "Input.h"
#include "Texture2D.h"
#include "Stage1Scene.h"
#include "Collision.h"
#include "ScreenFade.h"
#include "Oto.h"

namespace
{
	float s_LogoAngle = 0.0f;	//ロゴのy角度
}

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

	Oto::SetLogoPosition(Vector3(0.0f, 0.0f, 0.0f));
	Oto::SetLogoScale(30.0f);
	Oto::SetLogoAxis(Vector3(0.0f, 0.0f, 0.0f));
	Oto::SetLogoRotation(Vector3(0.0f, 0.0f, 0.0f));
	s_LogoAngle = 0.0f;
	auto addPart = [&](const char* file, const Vector3& spin, const char* tex)
		{
			auto* part = Game::GetInstance()->AddObject<Oto>();
			part->Load(file, "assets/model/title");
			if (tex) part->SetTexture(tex);
			else part->SetMatcap("assets/texture/matcap_gold.png");
			part->SetSpinSpeed(spin);
			m_MySceneObjects.emplace_back(part);
		};
	addPart("assets/model/title/oto.fbx", Vector3(0.0f, 0.0f, 0.0f), nullptr);	//音
	addPart("assets/model/title/ennyo.fbx", Vector3(0.0f, 0.0f, 0.0f),nullptr);	//延繞
	addPart("assets/model/title/kai_soto.fbx", Vector3(0.0f, 0.0f, -0.6f),nullptr);	//廻の外の円：ゆっくり右回り
	addPart("assets/model/title/kai_uti.fbx", Vector3(0.0f, 0.0f, 1.2f),nullptr);	//廻の内の円：逆向きに少し速く


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
	Game::GetInstance()->GetCamera()->SetFixedPose(Vector3(0.0f, 0.0f, -600.0f), Vector3(0.0f, 0.0f, 0.0f));
	m_LastClock = now;
	if (dt > 0.1f) dt = 0.1f;   // 読み込み直後などで大きく飛ばないように

	s_LogoAngle += DirectX::XM_PIDIV2 * dt;
	Oto::SetLogoRotation(Vector3(0.0f, s_LogoAngle, 0.0f));

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
			return;
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
