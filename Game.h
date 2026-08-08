#pragma once
#include <iostream>

#include "Renderer.h"
#include "TitleScene.h"
#include "Stage1Scene.h"
#include "ResultScene.h"
#include "Note.h"
#include "PostFlashEffect.h"

enum SceneName {
	TITLE,
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

	
	//オブジェクト配列
	std::vector<std::unique_ptr<Object>>m_Objects;
	std::vector<std::unique_ptr<Object>> m_AddBuffer;

public:
	Game(); // コンストラクタ
	~Game(); // デストラクタ

	static void Init(); // 初期化
	static void Update(); // 更新
	static void Draw(); // 描画
	static void Uninit(); // 終了処理

	static Game* GetInstance();
	Camera* GetCamera() { return &m_Camera; }
	Note* GetNote() { return &m_Note; }

	void ChangeScene(SceneName sName);	//シーンを変更
	void DeleteObject(Object* pt);		//オブジェクトを削除する
	void DeleteAllObject();				//オブジェクトをすべて削除

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
