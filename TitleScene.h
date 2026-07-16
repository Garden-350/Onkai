#pragma once
#include "Scene.h"
#include "Object.h"
#include "sound.h"


class Sound;
class TitleScene : public Scene
{
private:
	Sound sound;

	std::vector<Object*> m_MySceneObjects; // このシーンのオブジェクト

	void Init(); // 初期化
	void Uninit(); // 終了処理
public:
	TitleScene(); // コンストラクタ
	~TitleScene(); // デストラクタ

	void Update(); // 更新
};

