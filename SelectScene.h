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
	Texture2D* m_AutoButton = nullptr;
	Texture2D* m_HighSpeedDigits[2] = { nullptr, nullptr }; // [0]=整数部, [1]=小数第一位
	bool m_AutoModeSelected = false;

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

	void Update(); // 更新
};

