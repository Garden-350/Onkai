#pragma once
#include "Scene.h"
#include "Object.h"
#include "sound.h"
#include "ResultData.h"

class Texture2D;
// ResultSceneクラス
class ResultScene : public Scene
{
private:
	std::vector<Object*> m_MySceneObjects; // このシーンのオブジェクト
	std::vector<std::string> m_SongNames;
	Sound sound;

	void Init(); // 初期化
	void Uninit(); // 終了処理
	void SetUpDigitRow(Texture2D** row, int value, float x, float y);	// 3桁の数字テクスチャ配列を生成して、指定座標に指定値を表示する共通処理

	Texture2D* m_PerfectDigits[4] = { nullptr,nullptr,nullptr };
	Texture2D* m_GreatDigits[4] = { nullptr,nullptr,nullptr };
	Texture2D* m_GoodDigits[4] = { nullptr,nullptr,nullptr };
	Texture2D* m_MissDigits[4] = { nullptr, nullptr, nullptr };
	Texture2D* m_MaxComboDigits[4] = { nullptr, nullptr, nullptr };
	Texture2D* m_PressEnterObg = nullptr;

	ResultData m_Result;

	int m_SelectedIndex = 0;
	float m_PressEnterBlinkTimer = 0.0f;
	bool m_Initialized = false;
public:
	ResultScene(); // コンストラクタ
	~ResultScene(); // デストラクタ

	void Update(); // 更新
};

