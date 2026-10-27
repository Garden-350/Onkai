#include "ResultScene.h"
#include "Game.h"
#include "Input.h"
#include "Texture2D.h"
#include "sound.h"
#include "NumberDisplay.h"
#include "ScoreRule.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
	using Box = ResultScene::Box;

	//X,Y,縦の大きさ,横の大きさ

	//背景
	constexpr Box kBackdrop = { 0.0f,0.0f,1280.0f,720.0f };
	constexpr float kBackdropZ = 485.0f;	//奥に配置

	//左上：リトライ
	constexpr Box kRetryIcon = { -580.0f,300.0f,100.0f,100.0f };

	//左：曲のジャケット
	constexpr Box kJacket = { -400.0f,230.0f,200.0f,200.0f };

	//左：コンボ
	constexpr Box kComboLabel = { -400.0f,0.0f,200.0f,60.0f };
	constexpr Box kComboNumber = { -310.0f,-80.0f,80.0f,80.0f };

	//スコアとランク
	constexpr Box kScoreNumber = { 350.0f,250.0f,100.0f,100.0f };
	constexpr Box kRankImage = { -70.0f,250.0f,180.0f,180.0f };

	//判定（上からPerfect,Great,Good,Miss）
	constexpr float kJudgeTopY = 50.0f;		//perfectの行のy
	constexpr float kJudgeRowSpacing = 100.0f;	//行と行の間隔
	constexpr float kJudgeLabelX = 150.0f;		//「PERFECT」などの文字の中心X
	constexpr float kJudgeLabelW = 200.0f;		//文字の画像の幅
	constexpr float kJudgeLabelH = 60.0f;		//文字の画像の高さ
	constexpr float kJudgeNumberX = 440.0f;		//数字（4桁）の中心X
	constexpr float kJudgeDigitSize = 80.0f;	//数字１桁の大きさ

	//フルコンボの画像
	constexpr Box kFullCombo = { -400.0f,100.0f,360.0f,260.0f };

	//数字の見た目
	constexpr const char* kNumberSheet = "assets/texture/allnumber_new.png";
	constexpr float kLeadingZeroAlpha = 0.25f;	//頭の濃さ(0=消す,1=普通)
	constexpr float kCountSpeed = 100.0f;		//数える速さ(大きいほど速い)
	constexpr int kCountDigits = 4;				//判定数とコンボの桁数
	constexpr int kScoreDigits = 7;				//スコアの桁数
	constexpr float kFrontZ = -10.0f;			//数字とランクは手前に配置
}

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
	AddImage("assets/texture/stage_backdrop.png", kBackdrop, kBackdropZ);
}

// 更新
void ResultScene::Update()
{
	//最初の一回実行
	if (!m_Initialized)
	{
		BuildLayout();
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

void ResultScene::BuildLayout()
{
	m_Result = Game::GetResultData();

	//左上と左
	AddImage("assets/texture/retry_white.png", kRetryIcon);
	AddImage(FindJacketPath(m_Result.chartName).c_str(), kJacket);

	//コンボ
	AddImage("assets/texture/COMBO.png", kComboLabel);
	AddNumber(m_Result.maxCombo, kCountDigits, kComboNumber);

	//スコア
	int scoreValue = ScoreRule::Calc(m_Result.perfect, m_Result.great, m_Result.good, m_Result.totalNotes);
	AddNumber(scoreValue, kScoreDigits, kScoreNumber, true);
	bool newRecord = Game::RecordScore(m_Result.chartName, scoreValue);

	//ランク
	const char* rankImage = ScoreRule::RankImage(scoreValue);
	if (fs::exists(rankImage))
	{
		AddImage(rankImage, kRankImage, kFrontZ);
	}


	//判定
	AddJudgeRow("assets/texture/PERFECT.png", m_Result.perfect, 0);
	AddJudgeRow("assets/texture/GREAT.png", m_Result.great, 1);
	AddJudgeRow("assets/texture/GOOD.png", m_Result.good, 2);
	AddJudgeRow("assets/texture/MISS.png", m_Result.miss, 3);

	//フルコンボ
	if (m_Result.isFullCombo)
	{
		AddImage("assets/texture/FullCpmbo.png", kFullCombo);
	}
}

//box:中心の位置と大きさ
Texture2D* ResultScene::AddImage(const char* path, const Box& box, float z)
{
	Texture2D* tex = Game::GetInstance()->AddObject<Texture2D>();
	tex->SetTexture(path);
	tex->SetPosition(box.x, box.y, z);
	tex->SetScale(box.w, box.h, 0.0f);
	m_MySceneObjects.emplace_back(tex);
	return tex;
}

//AddNumber:数字を置く
//value:表示する値
//digits:桁数
//box:数字の列の中心の位置を一桁分の大きさ
//countUp:trueなら0から数えてfalseなら即表示
NumberDisplay* ResultScene::AddNumber(int value, int digits, const Box& box, bool countUp)
{
	NumberDisplay* num = Game::GetInstance()->AddObject<NumberDisplay>();
	num->SetUp(kNumberSheet, digits);
	num->SetDigitSize(box.w, box.h);		//桁の間隔も、幅の0.75倍で自動で決まる
	num->SetPosition(box.x, box.y, kFrontZ);
	num->SetLeadingZeroAlpha(kLeadingZeroAlpha);
	num->SetCountSpeed(kCountSpeed);

	if (countUp)
	{
		num->SetValue(0, true);		//まず0にしてから
		num->SetValue(value);		//本当の値を渡すと,そこまで数え上がる
	}
	else
	{
		num->SetValue(value, true);	//すぐ表示
	}

	m_MySceneObjects.emplace_back(num);
	return num;
}

//AddJudgeRow:判定の内訳を一行置く
//row:上から何行目か（0=一番上）yは行番号から自動で決定
void ResultScene::AddJudgeRow(const char* labelPath, int value, int row)
{
	float y = kJudgeTopY - kJudgeRowSpacing * row;	//下の行ほど y が小さい（下に行く）

	AddImage(labelPath, { kJudgeLabelX, y, kJudgeLabelW, kJudgeLabelH });
	AddNumber(value, kCountDigits, { kJudgeNumberX, y, kJudgeDigitSize, kJudgeDigitSize });
}

std::string ResultScene::FindJacketPath(const std::string& chartName)
{
	const char* exts[] = { ".png", ".jpeg", ".jpg" };
	for (const char* ext : exts)
	{
		std::string path = "assets/jacket/" + chartName + ext;
		if (fs::exists(path)) return path;
	}
	return "assets/jacket/default.png";
}
