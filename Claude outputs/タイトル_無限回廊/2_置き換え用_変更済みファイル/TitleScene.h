#pragma once
#include "Scene.h"
#include "Object.h"
#include "sound.h"

class TitleCorridor;
class ScreenFade;

class TitleScene : public Scene
{
private:
	Sound sound;

	std::vector<Object*> m_MySceneObjects; // このシーンのオブジェクト

	TitleCorridor* m_Corridor = nullptr;	// 背景の「たどり着けない回廊」
	ScreenFade* m_Fade = nullptr;			// 画面全体のフェード

	void Init(); // 初期化
	void Uninit(); // 終了処理
public:
	TitleScene(); // コンストラクタ
	~TitleScene(); // デストラクタ

	void Update(); // 更新
};
