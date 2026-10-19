#include "SelectScene.h"
#include "Game.h"
#include "Input.h"
#include "ChartObject.h"
#include "SkyBox.h"
#include "Enemy.h"
#include "Player.h"
#include "sound.h"
#include "SimpleJson.h"
#include "MagicCircleTransitionEffect.h"
#include "SpectrumRing.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;
namespace fs = std::filesystem;


SelectScene::SelectScene()
{
	Init();
}

SelectScene::~SelectScene()
{
	Uninit();
}


// 初期化
void SelectScene::Init()
{
	Renderer::SetDepthEnable(true);
	sound.Init();
	Texture2D* pt = Game::GetInstance()->AddObject<Texture2D>();
	pt->SetTexture("assets/texture/saiba-.jpg");
	pt->SetPosition(0.0f, 0.0f, 485.0f);
	pt->SetRotation(0.0f, 0.0f, 0.0f);
	pt->SetScale(1280.0f, 720.0f, 0.0f);
	m_MySceneObjects.emplace_back(pt);

	//透過する（たぶんリザルトとかで使ってるのが必須）　エンター押したら難易度選択して行く感じ？横に曲名出すかどうかは要検討
	//Renderer::SetBlendState(BS_NONE);
	//Texture2D* left = Game::GetInstance()->AddObject<Texture2D>();
	//left->SetTexture("assets/texture/nanido.png");
	//left->SetPosition(0.0f, -300.0f, 0.0f);
	//left->SetRotation(0.0f, 0.0f, 0.0f);
	//left->SetScale(400.0f, 200.0f, 0.0f);
	//m_MySceneObjects.emplace_back(left);

	// オートモード切替ボタン
	m_AutoModeSelected = false;
	m_AutoButton = Game::GetInstance()->AddObject<Texture2D>();
	m_AutoButton->SetTexture("assets/texture/Auto_off.png");
	m_AutoButton->SetPosition(500.0f, -280.0f, 0.0f);
	m_AutoButton->SetScale(180.0f, 80.0f, 0.0f);
	m_MySceneObjects.emplace_back(m_AutoButton);

	// ハイスピ表示（コンボと同じ数字画像を流用。小数点の絵は無いので2桁を隙間を空けて並べる）
	for (int i = 0; i < 2; i++)
	{
		m_HighSpeedDigits[i] = Game::GetInstance()->AddObject<Texture2D>();
		m_HighSpeedDigits[i]->SetTexture("assets/texture/allnumber_new.png");
		m_HighSpeedDigits[i]->SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示"0"
		m_HighSpeedDigits[i]->SetScale(60.0f, 60.0f, 0.0f);
		m_MySceneObjects.emplace_back(m_HighSpeedDigits[i]);
	}
	m_HighSpeedDigits[0]->SetPosition(480.0f, -180.0f, 0.0f);
	m_HighSpeedDigits[1]->SetPosition(530.0f, -180.0f, 0.0f); // 少し離して小数点っぽく見せる

	RefreshSongList();
	SetupJacketSlots();

	//円形スペクトラム
	m_Spectrum.Init(2048, 48);	//FFTサイズ2048,片側48本　両方合わせて円になるように
	m_SpectrumRing = Game::GetInstance()->AddObject<SpectrumRing>();
	m_MySceneObjects.emplace_back(m_SpectrumRing);
	//最初に選ばれている曲のプレビュー予約
	RequestPreview();

	m_TransitionEffect = Game::GetInstance()->AddObject<MagicCircleTransitionEffect>();
	m_MySceneObjects.emplace_back(m_TransitionEffect);
}



//更新
void SelectScene::Update()
{
	if (m_SongNames.empty()) return;

	// フレーム時間を計測
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockInitialized)
	{
		m_LastUpdateClock = now;
		m_ClockInitialized = true;
	}
	float dt = std::chrono::duration<float>(now - m_LastUpdateClock).count();
	m_LastUpdateClock = now;
	if (dt > 0.1f) dt = 0.1f;	//読み込みで止まったフレームの後に大きく飛ばないように
	//曲のプレビューとスペクトラム（遷移演出中も鳴らし続ける）
	UpdatePreview(dt);

	// ── 遷移演出中：魔法陣を描き進めるだけで、他の操作は受け付けない ──
	if (m_Transitioning)
	{
		m_TransitionTimer += dt;
		float progress = std::clamp(m_TransitionTimer / kTransitionDuration, 0.0f, 1.0f);
		if (m_TransitionEffect) m_TransitionEffect->SetProgress(progress);

		if (m_TransitionTimer >= kTransitionDuration)
		{
			Game::GetInstance()->ChangeScene(STAGE1);
		}
		return;
	}

	// Sキーで前の曲へ
	if (Input::GetKeyTrigger(VK_S) || Input::GetKeyTrigger(VK_LEFT))
	{
		ShiftJackets(-1);
	}
	// Dキーで次の曲へ
	if (Input::GetKeyTrigger(VK_D) || Input::GetKeyTrigger(VK_RIGHT))
	{
		ShiftJackets(1);
	}

	//ハイスピの設定
	if (Input::GetKeyTrigger(VK_L))
	{
		speed = std::clamp(Game::GetInstance()->GetHighSpeed() + 0.1f, 0.1f, 9.9f);
		Game::SetHightSpeed(speed);
	}
	if (Input::GetKeyTrigger(VK_K))
	{
		speed = std::clamp(Game::GetInstance()->GetHighSpeed() - 0.1f, 0.1f, 9.9f);
		Game::SetHightSpeed(speed);
	}
	//ハイスピ数字表示の更新
	{
		float hs = Game::GetInstance()->GetHighSpeed();
		int hsInt10 = (int)std::round(hs * 10.0f); // 浮動小数の誤差対策でまとめて丸める
		int intPart = (hsInt10 / 10) % 10;
		int decPart = hsInt10 % 10;

		m_HighSpeedDigits[0]->SetUV((float)(intPart + 1), 1.0f, 10.0f, 1.0f);
		m_HighSpeedDigits[1]->SetUV((float)(decPart + 1), 1.0f, 10.0f, 1.0f);
	}

	//TABでオート切替
	if (Input::GetKeyTrigger(VK_TAB))
	{
		m_AutoModeSelected = !m_AutoModeSelected;
		m_AutoButton->SetTexture(
			m_AutoModeSelected ? "assets/texture/Auto_on.png" : "assets/texture/Auto_off.png"
		);
	}

	UpdateJacketLayout(); // 毎フレーム補間を進める

	// エンターキーで決定してステージへ
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		//遷移アニメーションのところ。
		//Game::SetSelectedChart(m_SongNames[m_SelectedIndex]);
		//Game::SetAutoMode(m_AutoModeSelected);

		//m_Transitioning = true;
		//m_TransitionTimer = 0.0f;
		//if (m_TransitionEffect)
		//{
		//	m_TransitionEffect->SetVisible(true);
		//	m_TransitionEffect->SetProgress(0.0f);
		//}


		Game::SetSelectedChart(m_SongNames[m_SelectedIndex]);
		Game::SetAutoMode(m_AutoModeSelected);
		Game::GetInstance()->ChangeScene(STAGE1);
	}
}

// 終了処理
void SelectScene::Uninit()
{
	sound.Uninit();
	// このシーンのオブジェクトを削除する
	for (auto& o : m_MySceneObjects) {
		Game::GetInstance()->DeleteObject(o);
	}
	m_MySceneObjects.clear();
}

//chatr/フォルダの.jsonで曲名リストを作成
void SelectScene::RefreshSongList()
{
	m_SongNames.clear();

	const std::string chartsDir = "charts";
	if (fs::exists(chartsDir) && fs::is_directory(chartsDir))
	{
		for (const auto& entry : fs::directory_iterator(chartsDir))
		{
			if (!entry.is_regular_file()) continue;
			if (entry.path().extension() != ".json") continue;

			m_SongNames.push_back(entry.path().stem().string());
		}
	}

	//見つからなかった時の保険
	if (m_SongNames.empty())
	{
		m_SongNames.push_back("beam_2");	//動いてるフォルダを選択。あとで変えてもよし
	}

	//ソートする
	std::sort(m_SongNames.begin(), m_SongNames.end());

	m_SelectedIndex = 0;
}

// 5枚のジャケットスロットを生成し、初期位置(-2〜+2)に配置する
void SelectScene::SetupJacketSlots()
{
	m_JacketSlots.clear();
	m_JacketSlots.resize(kSlotCount);

	for (int i = 0; i < kSlotCount; i++)
	{
		int offset = i - kHalf;

		JacketSlot& slot = m_JacketSlots[i];
		slot.tex = Game::GetInstance()->AddObject<Texture2D>();
		slot.currentOffset = (float)offset;
		slot.targetOffset = (float)offset;
		slot.songIndex = WrapIndex(m_SelectedIndex + offset);

		LoadJacketTexture(slot);
		m_MySceneObjects.emplace_back(slot.tex);
	}
	UpdateJacketLayout();	//初回の座標を即反映
}

// +1で次の曲へ、-1で前の曲へ。全スロットのオフセットをずらし、
// 画面外に出たスロットは反対側へリサイクルして新しい曲を割り当てる
void SelectScene::ShiftJackets(int direction)
{
	if (m_SongNames.empty()) return;

	m_SelectedIndex = WrapIndex(m_SelectedIndex + direction);

	for (auto& slot : m_JacketSlots)
	{
		slot.targetOffset -= (float)direction;

		// 左端(-half)より外へ出たら、右端の外(+half+1)から出現させてリサイクル
		if (slot.targetOffset < -kHalf - 0.5f)
		{
			slot.currentOffset = (float)(kHalf + 1);
			slot.targetOffset = (float)kHalf;
			slot.songIndex = WrapIndex(m_SelectedIndex + kHalf);
			LoadJacketTexture(slot);
		}
		// 右端(+half)より外へ出たら、左端の外(-half-1)から出現させてリサイクル
		else if (slot.targetOffset > kHalf + 0.5f)
		{
			slot.currentOffset = (float)(-kHalf - 1);
			slot.targetOffset = (float)-kHalf;
			slot.songIndex = WrapIndex(m_SelectedIndex - kHalf);
			LoadJacketTexture(slot);
		}
	}
	RequestPreview();	//曲が変わったからプレビューも切替
}

void SelectScene::UpdateJacketLayout()
{
	for (auto& slot : m_JacketSlots)
	{
		// なめらかに目標位置へ近づける
		slot.currentOffset += (slot.targetOffset - slot.currentOffset) * kLerpSpeed;

		float offset = slot.currentOffset;
		float absOffset = fabsf(offset);

		// 中央(offset=0)ほど手前・大きく、外側ほど奥・小さく
		float x = offset * 220.0f + std::clamp(offset, -1.0f, 1.0f) * kSpectrumGap;
		float z = 300.0f + absOffset * 50.0f; // 奥へ行くほどzを大きくして遠ざける
		float scale = std::max(150.0f, 250.0f - absOffset * 90.0f);
		// 中央のジャケットだけ、低音に合わせて少し鼓動させる いらんかったらコメントアウト
		if (m_SpectrumRing && absOffset < 0.5f)
		{
			float weight = 1.0f - absOffset * 2.0f;	// 真ん中に来るほど強く
			scale *= 1.0f + 0.05f * m_SpectrumRing->GetBass() * weight;
		}

		slot.tex->SetPosition(x, 0.0f, z);
		slot.tex->SetScale(scale, scale, 0.0f);
	}
}

void SelectScene::LoadJacketTexture(JacketSlot& slot)
{
	if (slot.tex == nullptr) return;
	if (m_SongNames.empty()) return;

	const std::string& name = m_SongNames[slot.songIndex];
	const char* exts[] = { ".png", ".jpg", ".jpeg" };

	bool loaded = false;
	for (const char* ext : exts)
	{
		std::string path = "assets/jacket/" + name + ext;
		if (fs::exists(path))
		{
			slot.tex->SetTexture(path.c_str());
			loaded = true;
			break;
		}
	}

	if (!loaded)
	{
		std::string defaultPath = "assets/jacket/default.png";
		if (fs::exists(defaultPath))
		{
			slot.tex->SetTexture(defaultPath.c_str());
		}
	}
}

//曲の範囲でインデックス循環
int SelectScene::WrapIndex(int idx) const
{
	int count = (int)m_SongNames.size();
	if (count <= 0) return 0;
	int r = idx % count;
	if (r < 0) r += count;
	return r;
}

//連打中は読み込まない
void SelectScene::RequestPreview()
{
	sound.StopDynamic();
	m_PreviewPlaying = false;
	m_PreviewPending = true;
	m_PreviewTimer = kPreviewDelay;
}

void SelectScene::StartPreview(int songIndex)
{
	if (m_SongNames.empty()) return;

	std::string audioFile;
	float previewTime = -1.0f;
	if (!LoadPreviewInfo(m_SongNames[songIndex], audioFile, previewTime)) return;	//曲の設定がなかったら無音

	std::string fullPath = "assets/bgm/" + audioFile;
	if (FAILED(sound.LoadDynamic(fullPath, true))) return;

	float duration = sound.GetDynamicDurationSeconds();
	if (duration <= 0.0f) return;

	//開始位置　特に指定がなければ曲の3割りぐらいから
	float begin = (previewTime >= 0.0f) ? previewTime : duration * 0.3f;
	if (begin > duration) begin = 0.0f;
	float length = std::min(kPreviewLength, duration - begin);
	if (length < 3.0f)	//短い曲はまるごとループする
	{
		begin = 0.0f;
		length = duration;
	}
	sound.SetVolumeDynamic(0.0f);	//フェードインするから0から
	sound.PlayDynamicSection(begin, length);

	m_PreviewBegin = begin;
	m_PreviewLength = length;
	m_PreviewPlaying = true;
}

void SelectScene::UpdatePreview(float dt)
{
	//予約の消化
	if (m_PreviewPending)
	{
		m_PreviewTimer -= dt;
		if (m_PreviewTimer <= 0.0f)
		{
			m_PreviewPending = false;
			StartPreview(m_SelectedIndex);
		}
	}
	//ループの頭と終わりで音量フェード（ぶつ切り防止）
	float volume = 0.0f;
	if (m_PreviewPlaying && m_PreviewLength > 0.0f)
	{
		float t = sound.GetDynamicPlaySeconds() - m_PreviewBegin;	//区間内の経過秒
		volume = 1.0f;
		volume = std::min(volume, t / kPreviewFadeIn);
		volume = std::min(volume, (m_PreviewLength - t) / kPreviewFadeOut);
		volume = std::clamp(volume, 0.0f, 1.0f);
		sound.SetVolumeDynamic(volume * kPreviewVolume);
	}
	//スペクトラム
	m_Spectrum.Analyze(sound, m_SpectrumLevels);
	for (auto& v : m_SpectrumLevels) v *= volume;	//フェード中はバーも小さく
	if (m_SpectrumRing) m_SpectrumRing->SetLevels(m_SpectrumLevels, dt);
}

bool SelectScene::LoadPreviewInfo(const std::string& name, std::string& outAudioFile, float& outPreviewTime)
{
	outAudioFile.clear();
	outPreviewTime = -1.0f;

	JsonValue root;
	std::string err;
	if (!SimpleJson::ParseFile("charts/" + name + ".json", root, &err))
	{
		OutputDebugStringA(("[SelectScene] preview: chart parse failed: " + name + "\n").c_str());
		return false;
	}

	if (root.Has("meta"))
	{
		const JsonValue& meta = root["meta"];
		outAudioFile = meta["audioFile"].AsString("");
		outPreviewTime = meta["previewTime"].AsFloat(-1.0f);	// 譜面に "previewTime": 秒 があればそこから
	}
	if (outAudioFile.empty() && root.Has("audioFile"))
		outAudioFile = root["audioFile"].AsString("");

	return !outAudioFile.empty();
}

//選択中の曲名から　assets/jacket/内の画像を探してセット　.png .jpgどっちでも可
void SelectScene::UpdateJacket()
{
	if (m_SongNames.empty() || m_JacketTexture == nullptr) return;

	const std::string& name = m_SongNames[m_SelectedIndex];
	const char* exts[] = { ".png", ".jpg", ".jpeg" };

	bool loaded = false;
	for (const char* ext : exts)
	{
		std::string path = "assets/jacket/" + name + ext;
		if (fs::exists(path))
		{
			m_JacketTexture->SetTexture(path.c_str());
			loaded = true;
			break;
		}
	}

	//画像がなかったらデフォルト画像を入れる
	if (!loaded)
	{
		std::string defaultPath = "assets/jacket/default.png";
		if (fs::exists(defaultPath))
		{
			m_JacketTexture->SetTexture(defaultPath.c_str());
		}
	}
}

bool SelectScene::LoadChart(const std::string& name)
{
	JsonValue root;
	std::string err;

	// ── 音声ファイル名の読み込み ──
	if (root.Has("audioFile"))
		m_AudioFile = root["audioFile"].AsString("");
	else if (root.Has("meta") && root["meta"].Has("audioFile"))
		m_AudioFile = root["meta"]["audioFile"].AsString("");
	else
		m_AudioFile = "";

	if (!m_AudioFile.empty())
	{
		std::string fullPath = "assets/bgm/" + m_AudioFile; //曲を読み込んだ際は絶対にエディターで使ってる曲を入れること
		HRESULT hr = sound.LoadDynamic(fullPath, true);
		if (FAILED(hr))
			OutputDebugStringA(("[ChartObject] Failed to load audio: " + fullPath + "\n").c_str());
	}
	return true;
}


