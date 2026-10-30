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
#include "NumberDisplay.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include "ScoreRule.h"
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


void SelectScene::RefreshOptionButtons()
{
	m_AutoButton->SetTexture(m_AutoModeSelected ? "assets/texture/on.png" : "assets/texture/off.png");
	m_NoiseButton->SetTexture(m_NoiseModeSelected ? "assets/texture/on.png" : "assets/texture/off.png");
}

void SelectScene::UpdateOptionCursor(float dt)
{
	m_FrameY += (OptionRowY(m_OptionCursor) - m_FrameY) * 0.25f;
	m_FrameAnimTime += dt;
	m_OptionFrame->SetPosition(kOptionLabelX, m_FrameY, 10.0f);
	m_Tab->SetPosition(kTabX, m_FrameY + kOptionButtonDY, 0.0f);
	m_OptionFrame->SetAlpha(0.65f + 0.35f * sinf(m_FrameAnimTime * 5.0f));

	float autoA = (m_OptionCursor == 0) ? 1.0f : kDimAlpha;
	float noiseA = (m_OptionCursor == 1) ? 1.0f : kDimAlpha;
	m_AutoTexture->SetAlpha(autoA);
	m_AutoButton->SetAlpha(autoA);
	m_Noise->SetAlpha(noiseA);
	m_NoiseButton->SetAlpha(noiseA);

	//矢印の画像は、選んでいる行が変わった時だけ差し替える（毎フレーム読み込むと重いため）
	bool up = (m_OptionCursor == 0);	//オート選択中->ノイズの行に上矢印
	if (m_ArrowShownFor != m_OptionCursor)
	{
		m_OptionArrow->SetTexture(up ? "assets/texture/yazirusi_up.png" : "assets/texture/yazirusi_down.png");
		m_ArrowShownFor = m_OptionCursor;
	}

	//枠と反対の行の高さ（2つの行の高さの合計から枠の高さを引く。枠が下りると矢印は上る）
	float otherY = OptionRowY(0) + OptionRowY(1) - m_FrameY;

	//三角が画像の上か下に寄っているので、その分ずらして ON/OFF と高さを合わせる
	float fix = kOptionArrowSize * kArrowCenterFix * (up ? -1.0f : 1.0f);

	//指している向きへ小さくぴょこぴょこ（|sin| で、向きの方にだけ跳ねる）
	float bob = kArrowBob * fabsf(sinf(m_FrameAnimTime * 4.0f)) * (up ? 1.0f : -1.0f);

	m_OptionArrow->SetPosition(kTabX, otherY + kOptionButtonDY + fix + bob, 0.0f);
}

void SelectScene::UpdateSongInfo()
{
	if (m_SongNames.empty()) return;

	UpdateJacket();	//ジャケット（assets/music_name/<曲名>.png、無ければ default.png）

	const std::string& name = m_SongNames[m_SelectedIndex];
	std::string path = "assets/music_name/" + name + ".png";
	if (!fs::exists(path)) path = "assets/music_name/_unknown.png";	//なかったら「???」を表示
	if (fs::exists(path)) m_TitlePlate->SetTexture(path.c_str());

	//自己べ
	if (m_BestScore)
	{
		int best = Game::GetBestScore(name);
		m_HasBest = (best >= 0);
		m_BestScore->SetValue(0, true);
		m_BestScore->SetValue(m_HasBest ? best : 0);

		//ランク画像
		m_HasRank = false;
		if (m_HasBest && m_BestRank)
		{
			const char* rankImage = ScoreRule::RankImage(best);
			if (fs::exists(rankImage))
			{
				m_BestRank->SetTexture(rankImage);
				m_HasRank = true;
			}
		}
	}

	m_InfoAnim = 0.0f;	//滑り込みを最初から
}

void SelectScene::UpdateSongInfoAnim(float dt)
{
	m_InfoAnim = std::min(1.0f, m_InfoAnim + dt / kInfoAnimSec);
	float e = 1.0f - (1.0f - m_InfoAnim) * (1.0f - m_InfoAnim);	//最初は速く最後はゆっくり
	float slide = kInfoSlide * (1.0f - e);

	m_TitlePlate->SetPosition(kInfoX + slide, kTitleY, 100.0f);
	m_TitlePlate->SetAlpha(e);
	m_JacketTexture->SetPosition(kJacketX + slide, kJacketY, 100.0f);
	m_JacketTexture->SetAlpha(e);

	if (m_BestScore)
	{
		float y = kJacketY - kJacketSize * 0.5f - kBestScoreGap;
		float scoreX = kJacketX + slide + kBestScoreOffsetX;
		m_BestScore->SetPosition(scoreX, y, 90.0f);
		m_BestScore->SetAlpha(m_HasBest ? e : 0.0f);

		//スコアの左
		if (m_BestRank)
		{
			float rankX = scoreX - kBestDigit * 2.75f - kRankGap - kRankSize * 0.5f;
			m_BestRank->SetPosition(rankX, y, 90.0f);
			m_BestRank->SetAlpha(m_HasRank ? e : 0.0f);
		}
	}
}

// 初期化
void SelectScene::Init()
{
	Renderer::SetDepthEnable(true);
	sound.Init();
	Texture2D* pt = Game::GetInstance()->AddObject<Texture2D>();
	pt->SetTexture("assets/texture/haikei_1.png");
	pt->SetPosition(0.0f, 0.0f, 485.0f);
	pt->SetRotation(0.0f, 0.0f, 0.0f);
	pt->SetScale(1280.0f, 720.0f, 0.0f);
	m_MySceneObjects.emplace_back(pt);

	// ── オプション（上：オート / 下：ノイズ）──
	m_AutoModeSelected = Game::GetInstance()->IsAutoMode();		// 前に選んだ設定をそのまま表示する
	m_NoiseModeSelected = Game::GetInstance()->IsNoiseMode();
	m_OptionCursor = 0;
	m_FrameY = OptionRowY(0);

	// 枠は文字より先に作る（先に描いて、文字の後ろに出すため。z も少し奥にする）
	m_OptionFrame = Game::GetInstance()->AddObject<Texture2D>();
	m_OptionFrame->SetTexture("assets/texture/option_frame.png");
	m_OptionFrame->SetPosition(kOptionLabelX, m_FrameY, 10.0f);
	m_OptionFrame->SetScale(kFrameW, kFrameH, 0.0f);
	m_MySceneObjects.emplace_back(m_OptionFrame);

	//上の行：オート
	m_AutoTexture = Game::GetInstance()->AddObject<Texture2D>();
	m_AutoTexture->SetTexture("assets/texture/automode.png");
	m_AutoTexture->SetPosition(kOptionLabelX, OptionRowY(0) + kOptionLabelDY, 0.0f);
	m_AutoTexture->SetScale(400.0f, 100.0f, 0.0f);
	m_MySceneObjects.emplace_back(m_AutoTexture);

	m_AutoButton = Game::GetInstance()->AddObject<Texture2D>();
	m_AutoButton->SetPosition(kOptionButtonX, OptionRowY(0) + kOptionButtonDY, 0.0f);
	m_AutoButton->SetScale(80.0f, 60.0f, 0.0f);
	m_MySceneObjects.emplace_back(m_AutoButton);

	//下の行：ノイズ
	m_Noise = Game::GetInstance()->AddObject<Texture2D>();
	m_Noise->SetTexture("assets/texture/noisemode.png");
	m_Noise->SetPosition(kOptionLabelX, OptionRowY(1) + kOptionLabelDY, 0.0f);
	m_Noise->SetScale(400.0f, 100.0f, 0.0f);
	m_MySceneObjects.emplace_back(m_Noise);

	m_NoiseButton = Game::GetInstance()->AddObject<Texture2D>();
	m_NoiseButton->SetPosition(kOptionButtonX, OptionRowY(1) + kOptionButtonDY, 0.0f);
	m_NoiseButton->SetScale(80.0f, 60.0f, 0.0f);
	m_MySceneObjects.emplace_back(m_NoiseButton);

	RefreshOptionButtons();	//ON/OFF の画像を今の設定に合わせる

	m_Tab = Game::GetInstance()->AddObject<Texture2D>();
	m_Tab->SetTexture("assets/texture/tab.png");
	m_Tab->SetPosition(kTabX, m_FrameY + kOptionButtonDY, 0.0f);
	m_Tab->SetScale(80.0f, 60.0f, 0.0f);
	m_MySceneObjects.emplace_back(m_Tab);

	m_OptionArrow = Game::GetInstance()->AddObject<Texture2D>();
	m_OptionArrow->SetScale(kOptionArrowSize, kOptionArrowSize, 0.0f);
	m_MySceneObjects.emplace_back(m_OptionArrow);
	m_ArrowShownFor = -1;	//最初の UpdateOptionCursor で画像を読み込ませる

	//ハイスピ表示（コンボと同じ数字画像を流用。小数点の絵は無いので2桁を隙間を空けて並べる）
	for (int i = 0; i < 2; i++)
	{
		m_HighSpeedDigits[i] = Game::GetInstance()->AddObject<Texture2D>();
		m_HighSpeedDigits[i]->SetTexture("assets/texture/allnumber_new.png");
		m_HighSpeedDigits[i]->SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示"0"
		m_HighSpeedDigits[i]->SetScale(60.0f, 60.0f, 0.0f);
		m_MySceneObjects.emplace_back(m_HighSpeedDigits[i]);
	}
	m_HighSpeedDigits[0]->SetPosition(360.0f, -180.0f, 0.0f);
	m_HighSpeedDigits[1]->SetPosition(410.0f, -180.0f, 0.0f); // 少し離して小数点っぽく見せる

	RefreshSongList();
	SetupJacketSlots();

	//右側ジャケットと曲名の帯
	m_JacketTexture = Game::GetInstance()->AddObject<Texture2D>();
	m_JacketTexture->SetScale(kJacketSize, kJacketSize, 0.0f);
	m_MySceneObjects.emplace_back(m_JacketTexture);

	m_TitlePlate = Game::GetInstance()->AddObject<Texture2D>();
	m_TitlePlate->SetScale(kTitleW, kTitleH, 0.0f);
	m_MySceneObjects.emplace_back(m_TitlePlate);

	//ジャケットの下の自己べ
	m_BestScore = Game::GetInstance()->AddObject<NumberDisplay>();
	m_BestScore->SetUp("assets/texture/allnumber_new.png", 7);
	m_BestScore->SetDigitSize(kBestDigit, kBestDigit);
	m_BestScore->SetLeadingZeroAlpha(0.25f);
	m_BestScore->SetCountSpeed(120.0f);
	m_MySceneObjects.emplace_back(m_BestScore);

	//自己べのランク
	m_BestRank = Game::GetInstance()->AddObject<Texture2D>();
	m_BestRank->SetScale(kRankSize, kRankSize, 0.0f);
	m_MySceneObjects.emplace_back(m_BestRank);

	UpdateSongInfo();	// 最初の曲を表示

	//円形スペクトラム
	m_Spectrum.Init(2048, 48);	//FFTサイズ2048,片側48本　両方合わせて円になるように
	m_SpectrumRing = Game::GetInstance()->AddObject<SpectrumRing>();
	m_MySceneObjects.emplace_back(m_SpectrumRing);
	//最初に選ばれている曲のプレビュー予約
	RequestPreview();

	m_ArrowUp = Game::GetInstance()->AddObject<Texture2D>();
	m_ArrowUp->SetTexture("assets/texture/sankakuren_SP.png");
	m_ArrowUp->SetPosition(20.0f, 200.0f, 0.0f);
	m_ArrowUp->SetRotation(0.0f, 0.0f, 0.0f);
	m_ArrowUp->SetScale(105.0f, 202.0f, 0.0f);						// 1コマ分の大きさ
	m_ArrowUp->SetUV(1.0f, 1.0f, (float)kArrowFrames, 1.0f);	// 1コマ目、横3分割・縦1段
	m_MySceneObjects.emplace_back(m_ArrowUp);

	m_ArrowDown = Game::GetInstance()->AddObject<Texture2D>();
	m_ArrowDown->SetTexture("assets/texture/sankakuren_gyaku_SP.png");
	m_ArrowDown->SetPosition(20.0f, -200.0f, 0.0f);
	m_ArrowDown->SetRotation(0.0f, 0.0f, 0.0f);
	m_ArrowDown->SetScale(105.0f, 202.0f, 0.0f);						// 1コマ分の大きさ
	m_ArrowDown->SetUV(1.0f, 1.0f, (float)kArrowFramws_Down, 1.0f);	// 1コマ目、横3分割・縦1段
	m_MySceneObjects.emplace_back(m_ArrowDown);

	Texture2D* UI_A = Game::GetInstance()->AddObject<Texture2D>();
	UI_A->SetTexture("assets/UI/UI_S.png");
	UI_A->SetPosition(-50.0f, 150.0f, 0.0f);
	UI_A->SetScale(100.0f, 100.0f, 0.0f);
	m_MySceneObjects.emplace_back(UI_A);

	Texture2D* UI_D = Game::GetInstance()->AddObject<Texture2D>();
	UI_D->SetTexture("assets/UI/UI_F.png");
	UI_D->SetPosition(-50.0f, -150.0f, 0.0f);
	UI_D->SetScale(100.0f, 100.0f, 0.0f);
	m_MySceneObjects.emplace_back(UI_D);

	Texture2D* UI_J = Game::GetInstance()->AddObject<Texture2D>();
	UI_J->SetTexture("assets/UI/Down_J.png");
	UI_J->SetPosition(230.0f, -180.0f, 0.0f);
	UI_J->SetScale(150.0f, 150.0f, 0.0f);
	m_MySceneObjects.emplace_back(UI_J);

	Texture2D* UI_L = Game::GetInstance()->AddObject<Texture2D>();
	UI_L->SetTexture("assets/UI/High_L.png");
	UI_L->SetPosition(530.0f, -180.0f, 0.0f);
	UI_L->SetScale(150.0f, 150.0f, 0.0f);
	m_MySceneObjects.emplace_back(UI_L);

	Texture2D* High = Game::GetInstance()->AddObject<Texture2D>();
	High->SetTexture("assets/UI/highspeed.png");
	High->SetPosition(390.0f, -130.0f, 0.0f);
	High->SetScale(150.0f, 50.0f, 0.0f);
	m_MySceneObjects.emplace_back(High);

	Texture2D* Enter = Game::GetInstance()->AddObject<Texture2D>();
	Enter->SetTexture("assets/UI/enter.png");
	Enter->SetPosition(390.0f, -280.0f, 0.0f);
	Enter->SetScale(350.0f, 50.0f, 0.0f);
	m_MySceneObjects.emplace_back(Enter);

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
	UpdateSongInfoAnim(dt);

	m_ArrowAnimTime += dt;
	int frame = (int)(m_ArrowAnimTime / kArrowFrameSec) % kArrowFrames;
	m_ArrowUp->SetUV((float)(frame + 1), 1.0f, (float)kArrowFrames, 1.0f);	// コマ番号は1から数える

	m_ArrowAnimTime_Down += dt;
	int frame_down = (int)(m_ArrowAnimTime_Down / kArrowFrameSecDown) % kArrowFramws_Down;
	m_ArrowDown->SetUV((float)(frame_down + 1), 1.0f, (float)kArrowFramws_Down, 1.0f);

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
	if (Input::GetKeyTrigger(VK_F) || Input::GetKeyTrigger(VK_RIGHT))
	{
		ShiftJackets(1);
	}

	//ハイスピの設定
	if (Input::GetKeyTrigger(VK_L))
	{
		speed = std::clamp(Game::GetInstance()->GetHighSpeed() + 0.1f, 0.1f, 9.9f);
		Game::SetHightSpeed(speed);
	}
	if (Input::GetKeyTrigger(VK_J))
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

	//上下でオプションの行を選ぶ（2行なので、端まで行ったら反対側へ）
	if (Input::GetKeyTrigger(VK_UP))   m_OptionCursor = (m_OptionCursor + kOptionCount - 1) % kOptionCount;
	if (Input::GetKeyTrigger(VK_DOWN)) m_OptionCursor = (m_OptionCursor + 1) % kOptionCount;

	//TABで今選んでいる行の ON/OFF を切り替え
	if (Input::GetKeyTrigger(VK_TAB))
	{
		if (m_OptionCursor == 0) m_AutoModeSelected = !m_AutoModeSelected;
		else                     m_NoiseModeSelected = !m_NoiseModeSelected;
		RefreshOptionButtons();
	}
	UpdateOptionCursor(dt);

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
		Game::SetNoiseMode(m_NoiseModeSelected);
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
	UpdateSongInfo();	// 右側の曲名とジャケットも切り替え
}

void SelectScene::UpdateJacketLayout()
{
	for (auto& slot : m_JacketSlots)
	{
		// なめらかに目標位置へ近づける
		slot.currentOffset += (slot.targetOffset - slot.currentOffset) * kLerpSpeed;

		float offset = slot.currentOffset;
		float absOffset = fabsf(offset);

		//円周上の角度。前の曲(offsetが-)ほど上　次の曲(+)ほど上
		float angle = -offset * kWheelStep;
		float x = kWheelCenterX + cosf(angle) * kWheelRadius;
		float y = kWheelCenterY + sinf(angle) * kWheelRadius;
		float z = 300.0f + absOffset * 50.0f;	//選択中　手前

		float s = std::max(0.5f, 1.0f - absOffset * 0.25f);	//選択中ほど大きくする
		if (m_SpectrumRing && absOffset < 0.5f)
		{
			float weight = 1.0f - absOffset * 2.0f;
			s *= 1.0f + 0.05f * m_SpectrumRing->GetBass() * weight;	//低音に合わせてばくばくする
		}
		slot.tex->SetPosition(x, y, z);
		slot.tex->SetRotation(0.0f, 0.0f, DirectX::XMConvertToDegrees(angle));	// SetRotation は「度」で受け取る
		slot.tex->SetScale(kCardW * s, kCardH * s, 0.0f);
		slot.tex->SetAlpha(std::clamp(1.0f - absOffset * 0.3f, 0.0f, 1.0f));	//外側ほど薄くする
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
	m_SpectrumRing->SetCenter(DirectX::SimpleMath::Vector3(kWheelCenterX + kWheelRadius, kWheelCenterY, 450.0f));
	m_SpectrumRing->SetRadius(360.0f);
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


