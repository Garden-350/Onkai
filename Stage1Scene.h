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
	void ApplyCamera();

	ChartObject* m_Chart = nullptr;
	class SpotlightRow* m_Spotlights = nullptr;	//ランウェイ風なスポットライトを追加したい
	bool m_AudioFinished = false;
	float m_FinishWaitTimer = 0.0f;
	static constexpr float kResultDelaySeconds = 2.0f;	//曲が終わってから何秒待つか

	//元のカメラの座標(0,-535,-210)これをそれぞれ計算する
	float m_CamHeight = 980.0f;	//判定ラインからの高さ     (デバッグログの2つめの数値)-(-535)
	float m_CamBack = 670.0f;	//判定ラインから手前への距離	-210-(でばっブログの3つめの数値)
	float m_CamLookAhead = 500.0f;	//判定ラインよりどれだけ奥を見るか
public:
	Stage1Scene(); // コンストラクタ
	~Stage1Scene(); // デストラクタ

	void Update(); // 更新
};