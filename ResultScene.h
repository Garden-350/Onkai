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
	Sound sound;

	void Init(); // 初期化
	void Uninit(); // 終了処理
	void SetUpDigitRow(Texture2D** row, int value, float x, float y);	// 3桁の数字テクスチャ配列を生成して、指定座標に指定値を表示する共通処理

	Texture2D* m_PerfectDigits[3] = { nullptr,nullptr,nullptr };
	Texture2D* m_GreatDigits[3] = { nullptr,nullptr,nullptr };
	Texture2D* m_GoodDigits[3] = { nullptr,nullptr,nullptr };
	Texture2D* m_MissDigits[3] = { nullptr, nullptr, nullptr };
	Texture2D* m_MaxComboDigits[3] = { nullptr, nullptr, nullptr };

	ResultData m_Result;
public:
	ResultScene(); // コンストラクタ
	~ResultScene(); // デストラクタ

	void Update(); // 更新
};

