#pragma once
#include "Scene.h"
#include "Object.h"
#include "sound.h"
#include "AudioSpectrum.h"
#include <vector>
#include <string>
#include <chrono>

class Texture2D;
class Sound;
class MagicCircleTransitionEffect;
class SpectrumRing;
class NumberDisplay;
class SelectScene : public Scene
{
private:
	//ジャケット1枚分の状態
	struct JacketSlot
	{
		Texture2D* tex = nullptr;
		int songIndex = 0;			//このスロットが表示している曲のインデックス
		float currentOffset = 0.0f;	//現在の中央からの相対位置
		float targetOffset = 0.0f;	//目標の相対位置
	};
	static constexpr int kSlotCount = 5;	//表示するジャケット数
	static constexpr int kHalf = kSlotCount / 2;	//中央から見た最大オフセット
	static constexpr float kLerpSpeed = 0.2f;	//アニメーション速度

	Sound sound;

	std::vector<Object*> m_MySceneObjects; // このシーンのオブジェクト
	std::vector<std::string> m_SongNames;
	std::vector<JacketSlot> m_JacketSlots;
	std::string m_AudioFile;
	int m_SelectedIndex = 0;

	Texture2D* m_JacketTexture = nullptr;
	Texture2D* m_AutoTexture = nullptr;
	Texture2D* m_Noise = nullptr;
	Texture2D* m_AutoButton = nullptr;
	Texture2D* m_HighSpeedDigits[2] = { nullptr, nullptr }; // [0]=整数部, [1]=小数第一位
	bool m_AutoModeSelected = false;
	bool m_NoiseModeSelected = true;

	//矢印（TAB が無い方の行に出す）
	Texture2D* m_OptionArrow = nullptr;
	int   m_ArrowShownFor = -1;		//今の矢印の画像を、どの行を選んでいる時用に読み込んだか
	static constexpr float kOptionArrowSize = 70.0f;	//矢印の大きさ
	static constexpr float kArrowCenterFix = 0.19f;		//画像の中で三角が上下に寄っている分（大きさに対する割合）
	static constexpr float kArrowBob = 4.0f;			//矢印がぴょこぴょこ動く幅

	//オプション（上：オート / 下：ノイズ）
	Texture2D* m_NoiseButton = nullptr;		//ノイズの ON/OFF
	Texture2D* m_OptionFrame = nullptr;		//今選んでいる行を囲む枠
	int   m_OptionCursor = 0;				//0 = オート（上）、1 = ノイズ（下）
	float m_FrameY = 0.0f;					//枠の今の高さ（なめらかに動かす）
	float m_FrameAnimTime = 0.0f;			//枠の点滅用

	//TABのテクスチャ
	Texture2D* m_Tab = nullptr;

	static constexpr int   kOptionCount = 2;
	static constexpr float kOptionLabelX = 500.0f;		//文字（automode / noisemode）の中心X
	static constexpr float kOptionButtonX = 430.0f;		//ON/OFF の中心X
	static constexpr float kOptionRowY0 = 170.0f;		//1行目（オート）の真ん中の高さ
	static constexpr float kOptionRowGap = 150.0f;		//行と行の間
	static constexpr float kOptionLabelDY = 30.0f;		//行の真ん中から文字までのずれ
	static constexpr float kOptionButtonDY = -35.0f;	//行の真ん中から ON/OFF までのずれ
	static constexpr float kFrameW = 280.0f;			//枠の大きさ
	static constexpr float kFrameH = 150.0f;
	static constexpr float kDimAlpha = 0.45f;			//選んでいない行の濃さ
	static constexpr float kTabX = 550.0f;	//TAB の画像の中心X（ON/OFF の右）
	static float OptionRowY(int i) { return kOptionRowY0 - kOptionRowGap * i; }	//i 行目の真ん中の高さ

	void RefreshOptionButtons();	//ON/OFFの画像を今の設定に合わせる
	void UpdateOptionCursor(float dt);	//枠と行の濃さを毎フレーム更新

	//遷移アニメーション
	MagicCircleTransitionEffect* m_TransitionEffect = nullptr;
	bool m_Transitioning = false;
	float m_TransitionTimer = 0.0f;
	static constexpr float kTransitionDuration = 1.2f;	//遷移の長さ(s)

	std::chrono::steady_clock::time_point m_LastUpdateClock;
	bool m_ClockInitialized = false;

	//曲のプレビュー再生
	//ジャケット切り替え時に連打で重くなるので選択がkPreviewDelay(s)止まってから読み込み再生
	bool m_PreviewPending = false;	//読み込み待ち中か
	float m_PreviewTimer = 0.0f;	//読み込みまでの残り秒数
	bool m_PreviewPlaying = false;	//再生中か
	float m_PreviewBegin = 0.0f;	//ループ区間の開始
	float m_PreviewLength = 0.0f;	//ループ区間の長さ
	static constexpr float kPreviewDelay = 0.35f;	// 選択が止まってから再生を始めるまでの秒数
	static constexpr float kPreviewLength = 20.0f;	//ループの長さ
	static constexpr float kPreviewFadeIn = 0.6f;	//ループ頭のフェードイン
	static constexpr float kPreviewFadeOut = 1.0f;	//ループ終わりのフェードアウト
	static constexpr float kPreviewVolume = 0.8f;	//プレビュー時の音量

	//円形スペクトラムの表示
	AudioSpectrum m_Spectrum;	//FFT解析を使用 （今流してる曲の振れ幅とかを高速で検知）
	std::vector<float> m_SpectrumLevels;	//解析結果(帯域ごとの0~1)
	SpectrumRing* m_SpectrumRing = nullptr;	//描画
	static constexpr float kSpectrumGap = 180.0f;	//中央ジャケットと両隣の間に追加で空ける幅

	//曲選択ホイール
	static constexpr float kWheelCenterX = -560.0f;	//円の中心（画面の左に位置）
	static constexpr float kWheelCenterY = 0.0f;
	static constexpr float kWheelRadius = 300.0f;	//半径。選択中の曲は中心の真右に来る
	static constexpr float kWheelStep = 0.40f;		//隣の曲との角度
	static constexpr float kCardW = 320.0f;			//選択中のカードの横幅
	static constexpr float kCardH = 140.0f;			//選択中のカードの高さ

	//右側　曲情報表示するやつ
	Texture2D* m_TitlePlate = nullptr;	//曲名の帯
	float m_InfoAnim = 1.0f;			//曲を切り替えたときの滑り込み
	static constexpr float kInfoX = 370.0f;	//曲名の帯の中心X
	static constexpr float kTitleY = 262.0f;
	static constexpr float kTitleW = 520.0f;	//曲の画像サイズが違う場合はここを調整
	static constexpr float kTitleH = 85.0f;
	static constexpr float kJacketX = 220.0f;
	static constexpr float kJacketY = 105.0f;
	static constexpr float kJacketSize = 210.0f;
	static constexpr float kInfoSlide = 40.0f;		//右からどれぐらい滑り込むか
	static constexpr float kInfoAnimSec = 0.25f;	//滑り込みに掛ける秒数
	void UpdateSongInfo();				// 選択中の曲の曲名・ジャケットに差し替える
	void UpdateSongInfoAnim(float dt);	// 毎フレーム：滑り込みとフェードイン

	//スコア表示
	NumberDisplay* m_BestScore = nullptr;	//ジャケットの下に自己べ表示
	static constexpr float kBestDigit = 50.0f;	//数字一桁の大きさ
	static constexpr float kBestScoreGap = 45.0f;	//ジャケットの下端から数字の真ん中までの距離
	bool m_HasBest = false;						//プレイ済み？

	//ランク表示
	Texture2D* m_BestRank = nullptr;	//自己べの時のランク
	static constexpr float kBestScoreOffsetX = 150.0f;	//ジャケットの真ん中からスコアの真ん中までの横ずれ
	static constexpr float kRankSize = 90.0f;			//ランク画像の大きさ
	static constexpr float kRankGap = 20.0f;			//スコアの左端とランク画像の間の間隔
	bool m_HasRank = false;	//ランク出すか

	void Init(); // 初期化
	void Uninit(); // 終了処理
	void RefreshSongList();	//charts/内の.jsonを並べてm_SongNamesを作成
	void UpdateJacket();      // 現在選択中の曲に合わせてジャケット画像を差し替える
	void SetupJacketSlots();                // 5枚のTexture2Dを生成し初期配置する
	void ShiftJackets(int direction);        // +1:次の曲へ / -1:前の曲へ。全スロットをずらす
	void UpdateJacketLayout();               // 毎フレーム呼ぶ。補間とpos/scaleの反映
	void LoadJacketTexture(JacketSlot& slot); // songIndexに応じた画像をロード(無ければdefault)
	int  WrapIndex(int idx) const;           // 曲リストの範囲でインデックスを循環させる
	void RequestPreview();                   // 今の曲のプレビューを少し待ってから再生する予約
	void StartPreview(int songIndex);        // 実際に曲を読み込んでループ再生を始める
	void UpdatePreview(float dt);            // 毎フレーム：予約の消化・音量フェード・スペクトラム更新
	bool LoadPreviewInfo(const std::string& name, std::string& outAudioFile, float& outPreviewTime); // 譜面JSONから曲ファイル名を読む
public:
	SelectScene(); // コンストラクタ
	~SelectScene(); // デストラクタ

	bool LoadChart(const std::string& name);

	float speed = 0.0f;

	Texture2D* m_ArrowUp = nullptr;				// 上の三角（横3コマのスプライトシート）
	float m_ArrowAnimTime = 0.0f;				// アニメーションの経過時間
	static constexpr int kArrowFrames = 3;		// シートのコマ数（横に3つ）
	static constexpr float kArrowFrameSec = 0.5f;	// 1コマを何秒見せるか

	Texture2D* m_ArrowDown = nullptr;
	float m_ArrowAnimTime_Down = 0.0f;
	static constexpr int kArrowFramws_Down = 3;
	static constexpr float kArrowFrameSecDown = 0.5f;

	void Update(); // 更新
};

