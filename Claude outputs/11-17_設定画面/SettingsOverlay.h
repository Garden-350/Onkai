#pragma once
// ============================================================
// SettingsOverlay : 曲選びで Space を押すと出る「設定」の画面
//   ・↑↓ で項目をえらぶ、←→ で値を変える（Shift を押しながらだと大きく）
//   ・TAB：タイミングを測る（メトロノームに合わせて S D F J K L を押す）
//   ・Space：閉じる（閉じた時に player_settings.json に保存）
//   ・どの画面よりも手前に描く（深度を見ない）。開いている間、曲選びの操作は止める
//
// 項目を足すとき
//   1. Item に 1 つ足す（kItemCount も増える）
//   2. ラベルの絵を assets/settings/ に置いて、Init でラベルを読む
//   3. ChangeValue と DrawValue に、その項目の分を書く
// ============================================================
#include "Object.h"
#include "Texture2D.h"
#include "NumberDisplay.h"
#include <chrono>
#include <vector>

class Sound;

class SettingsOverlay : public Object
{
public:
	void Init() override;
	void Update() override {}			// 入力は Tick で（曲選びから呼ぶ）
	void Draw(Camera* cam) override;
	void Uninit() override {}
	Camera::Layer GetLayer() const override { return Camera::Layer::Playfield; }	// 曲選びの物より後に描く

	// 曲選びから毎フレーム呼ぶ。sound はメトロノームを鳴らすのに使う（曲のプレビューと同じ物）
	void Tick(float dt, Sound& sound);

	void Open();
	void Close();
	bool IsOpen() const { return m_Mode != Mode::Closed; }

private:
	enum class Mode { Closed, Menu, Calib };
	enum Item { kBrightness, kSpeed, kTiming, kItemCount };

	void ChangeValue(int item, int dir, bool big);	// ←→ で値を変える
	void ApplyToGame();								// ハイスピードなど、ゲームの方にも反映

	// ---- タイミングを測る ----
	void StartCalib(Sound& sound);
	void TickCalib(Sound& sound);
	void StopCalib(Sound& sound);

	// ---- 描く ----
	void DrawMenu(Camera* cam);
	void DrawCalib(Camera* cam);
	void DrawSigned(Camera* cam, float x, float y, int value, float digitSize);	// ±付きの数字 ＋ ms

	Mode m_Mode = Mode::Closed;
	int m_Cursor = 0;
	float m_Anim = 0.0f;

	// メトロノーム（タイミングを測る）
	static constexpr float kCalibBpm = 120.0f;			// メトロノームの音（assets/se/calib_metronome.wav）と同じにする
	static constexpr double kCalibLead = 1.0;			// 最初のクリックまでの無音（秒）
	static constexpr int kCalibPractice = 4;			// 最初の 4 拍は練習
	static constexpr int kCalibCount = 32;				// 数える拍
	static constexpr int kCalibMinTaps = 8;				// これより少なければ結果を出さない
	bool m_CalibRunning = false;
	bool m_CalibDone = false;
	std::chrono::steady_clock::time_point m_CalibStart;	// メトロノームを鳴らせと言った時刻（ゲームと同じ数え方）
	std::vector<double> m_CalibTaps;					// 拍とのずれ（ms）
	double m_CalibResult = 0.0;
	double m_CalibFlash = 0.0;							// 押した時に光らせる残り時間

	// 絵
	Texture2D m_Dim;					// 後ろを暗くする黒い板（画面いっぱい）
	Texture2D m_Panel, m_Title, m_Hint, m_Frame;
	Texture2D m_Labels[kItemCount];
	Texture2D m_Percent, m_Dot, m_Ms, m_Plus, m_Minus;
	Texture2D m_CalibTitle, m_CalibHint, m_CalibDoneHint, m_CalibFew, m_LabelCount, m_LabelResult, m_TapFlash;
	NumberDisplay m_Num3, m_Num2;		// 3 桁（明るさ・ms）、2 桁（ハイスピード・回数）
};
