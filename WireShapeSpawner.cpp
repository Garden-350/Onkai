#include "WireShapeSpawner.h"
#include "WireShape.h"
#include "Game.h"
#include <cstdlib>

using namespace DirectX::SimpleMath;

void WireShapeSpawner::Init()
{
}

void WireShapeSpawner::Update()
{
	if (!m_pSceneObjects)return;

	m_SpawnTimer += Game::GetInstance()->GetDeltaTime();
	if (m_SpawnTimer < m_SpawnInterval)
	{
		//画面外に出たShapeを削除
		for (auto it = m_pSceneObjects->begin(); it != m_pSceneObjects->end();)
		{
			WireShape* shape = dynamic_cast<WireShape*>(*it);
			if (shape && shape->IsOutOfScreen())
			{
				shape->Kill();
				it = m_pSceneObjects->erase(it);
			}
			else ++it;
		}
		return;
	}
	if (m_SpawnTimer < m_SpawnInterval) return;
	m_SpawnTimer = 0.0f;

	//ランダム生成
	float w = 50.0f + (rand() % 200);
	float h = 50.0f + (rand() % 200);
	float y = -200.0f + (rand() % 300);
	float speed = 200.0f + (rand() % 300);
	float r = (float)(rand() % 100) / 100.0f * 0.3f;
	float g = (float)(rand() % 100) / 100.0f;
	Color col(r, g, 1.0f, 0.8f);
	float side = (rand() % 2 == 0) ? -300.0f : 300.0f;	//両壁のどっちかに配置
	
	WireShape* shape = Game::GetInstance()->AddObject<WireShape>();
	shape->SetUp(side, y, 1200.0f, w, h, col, speed);

	float rx = 1.57f;						//横向きに出るように調整
	float ry = ((rand() % 60) - 30) * 0.01f;
	float rz = ((rand() % 60) - 30) * 0.01f;
	shape->SetRot(rx, ry, rz);

	m_pSceneObjects->emplace_back(shape);
}

void WireShapeSpawner::Draw(Camera* cam)
{
}

void WireShapeSpawner::Uninit()
{
}
