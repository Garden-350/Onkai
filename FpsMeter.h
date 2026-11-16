#pragma once
#include "NumberDisplay.h"
#include <chrono>
#include <string>

//FPS と 1 フレームの処理時間を測って、画面の左上とウィンドウのタイトルに出す
class FpsMeter
{
public:
	static constexpr int kToggleKey = 0x78;	//VK_F9。表示の切り替え（F12 は VS のデバッガーが止まるので使わない）

	void Init();
	void BeginFrame();		//Game::Update の一番最初
	void EndFrame();		//Game::Draw の最後（画面に出す前）
	void Draw(Camera* cam);	//Game::Draw の最後
	void Toggle();

private:
	using Clock = std::chrono::steady_clock;

	NumberDisplay m_FpsDigits;	//上の段：FPS
	NumberDisplay m_MsDigits;	//下の段：一番重かったフレームの処理時間(ms)

	Clock::time_point m_FrameStart;		//今のフレームの始まり
	Clock::time_point m_WindowStart;	//集計を始めた時刻
	bool m_Started = false;
	bool m_Visible = false;

	int m_Frames = 0;		//集計中のフレーム数
	float m_WorkSum = 0.0f;	//処理時間の合計(ms)
	float m_WorkMax = 0.0f;	//一番重かったフレーム(ms)

	std::wstring m_BaseTitle;	//元のウィンドウのタイトル

	static constexpr float kInterval = 0.5f;	//何秒ごとに数字を更新するか
};