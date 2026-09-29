#pragma once
#include "Scene.h"
#include "Object.h"
#include "sound.h"
#include "TitleLake.h"

class Texture2D;
class TitleLake;
class ScreenFade;
class Sound;
class TitleScene : public Scene
{
private:
	Sound sound;

	std::vector<Object*> m_MySceneObjects; // このシーンのオブジェクト

	TitleLake* m_Lake = nullptr;	//背景の湖
	ScreenFade* m_Fade = nullptr;	//画面全体へのフェード
	std::vector<Texture2D*>m_Logo;	//タイトルのパーツ

	Texture2D* kai_in = nullptr;
	Texture2D* kai_out = nullptr;

	void Init(); // 初期化
	void Uninit(); // 終了処理

	float m_kai_in_z = 0.0f;
	float m_kai_out_z = 0.0f;
public:
	TitleScene(); // コンストラクタ
	~TitleScene(); // デストラクタ

	void Update(); // 更新
};

