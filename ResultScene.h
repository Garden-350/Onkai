#pragma once
#include "Scene.h"
#include "Object.h"
#include "sound.h"
#include "ResultData.h"
#include <string>

class Texture2D;
class NumberDisplay;
// ResultSceneクラス
class ResultScene : public Scene
{
public:
	struct Box
	{
		float x, y;	//中心の位置
		float w, h;	//大きさ
	};

	ResultScene(); // コンストラクタ
	~ResultScene(); // デストラクタ

	void Update(); // 更新

	bool defeated = false;	//倒したか
private:
	std::vector<Object*> m_MySceneObjects; // このシーンのオブジェクト
	Sound sound;

	void Init(); // 初期化
	void Uninit(); // 終了処理
	void BuildLayout();	//リザルトの中身を並べる　Updateで一回実行

	// 画像を1枚置く
	Texture2D* AddImage(const char* path, const Box& box, float z = 0.0f);
	// 数字を置く。countUp = true なら 0 から数え上がる
	NumberDisplay* AddNumber(int value, int digits, const Box& box, bool countUp = false);
	// 判定の内訳を1行置く（ラベルの画像＋数字）。row は上から 0,1,2,3
	void AddJudgeRow(const char* labelPath, int value, int row);
	// 曲のジャケット画像を探す（無ければ default.png）
	static std::string FindJacketPath(const std::string& chartName);

	ResultData m_Result;

	std::vector<std::string> m_SongNames;
	Texture2D* m_PressEnterObg = nullptr;
	int   m_SelectedIndex = 0;
	float m_PressEnterBlinkTimer = 0.0f;

	bool m_Initialized = false;	// BuildLayout を呼んだか
};

