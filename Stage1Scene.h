#pragma once
#include "Scene.h"
#include "Object.h"
#include "sound.h"
#include <chrono>
#include "ChartObject.h"

// Stage1Scenクラス
class Stage1Scene : public Scene
{
private:

	Sound sound;
	std::vector<Object*> m_MySceneObjects; // このシーンのオブジェクト
	std::chrono::steady_clock::time_point m_LastUpdateClock;
	bool m_ClockInitialized = false;

	void Init(); // 初期化
	void Uninit(); // 終了処理
	void SendResult();

	ChartObject* m_Chart = nullptr;
	bool m_AudioFinished = false;
	float m_FinishWaitTimer = 0.0f;
	static constexpr float kResultDelaySeconds = 2.0f;	//曲が終わってから何秒待つか

public:
	Stage1Scene(); // コンストラクタ
	~Stage1Scene(); // デストラクタ

	void Update(); // 更新
};