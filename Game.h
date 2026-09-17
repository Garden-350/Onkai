#pragma once
#include <iostream>

#include "Renderer.h"
#include "TitleScene.h"
#include "SelectScene.h"
#include "Stage1Scene.h"
#include "ResultScene.h"
#include "Note.h"
#include "PostFlashEffect.h"
#include "ResultData.h"

enum SceneName {
	TITLE,
	SELECT,
	STAGE1,
	RESULT
};



class Game
{
private:
	static Game* m_Instance;	//ゲームインスタンス

	Scene* m_Scene;	//シーン

	// カメラ
	Camera m_Camera;

	Note m_Note;
	PostFlashEffect m_PostFlash;

	ResultData m_LastResult;

	float m_DeltaTime = 0.0f;

	//オブジェクト配列
	std::vector<std::unique_ptr<Object>>m_Objects;
	std::vector<std::unique_ptr<Object>> m_AddBuffer;
	std::string m_SelectedChart = "beam_2";

	bool m_AutoMode = false;
	bool m_Paused = false;
	std::chrono::steady_clock::time_point m_PauseStartClock{};
	float m_HighSpeed = 1.0f;

public:
	Game(); // コンストラクタ
	~Game(); // デストラクタ

	static void Init(); // 初期化
	static void Update(); // 更新
	static void Draw(); // 描画
	static void Uninit(); // 終了処理

	static void SetSelectedChart(const std::string& name) { m_Instance->m_SelectedChart = name; }
	static const std::string& GetSelectedChart() { return m_Instance->m_SelectedChart; }

	static void SetAutoMode(bool enable) { m_Instance->m_AutoMode = enable; }
	bool IsAutoMode() { return m_Instance->m_AutoMode; }

	static void SetResultData(const ResultData& data) { m_Instance->m_LastResult = data; }
	static const ResultData& GetResultData() { return m_Instance->m_LastResult; }

	static Game* GetInstance();
	Camera* GetCamera() { return &m_Camera; }
	Note* GetNote() { return &m_Note; }

	void ChangeScene(SceneName sName);	//シーンを変更
	void DeleteObject(Object* pt);		//オブジェクトを削除する
	void DeleteAllObject();				//オブジェクトをすべて削除

	float GetDeltaTime() const { return m_DeltaTime; }

	bool IsPaused() const { return m_Instance->m_Paused; }

	float GetHighSpeed() const { return m_Instance->m_HighSpeed; }
	static void SetHightSpeed(float speed) { m_Instance->m_HighSpeed = speed; }

	//オブジェクトを追加する（テンプレート関数）
	//template<typename T> T* AddObject()
	//{
	//	T* pt = new T;
	//	m_Instance->m_Objects.emplace_back(pt);
	//	pt->Init();	//初期化
	//	return pt;
	//}
	template<typename T>
	T* AddObject()
	{
		T* pt = new T;
		m_Instance->m_AddBuffer.emplace_back(pt); 
		pt->Init();
		return pt;
	}

	//オブジェクトを取得
	template<typename T> std::vector<T*> GetObjects()
	{
		std::vector<T*> res;
		for (auto& o : m_Instance->m_Objects)
		{
			//dynamic_castで型をチェック
			if (T* derivedObj = dynamic_cast<T*>(o.get()))
			{
				res.emplace_back(derivedObj);
			}
		}
		return res;
	}
};
