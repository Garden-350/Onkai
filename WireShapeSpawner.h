#pragma once
#include "Object.h"
#include <vector>

class WireShapeSpawner : public Object
{
private:
	float m_SpawnTimer = 0.0f;
	float m_SpawnInterval = 0.8f;
	std::vector<class Object*>* m_pSceneObjects = nullptr;

public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	void SetSceneObjects(std::vector<class Object*>* list) { m_pSceneObjects = list; }
	void SetInterval(float interval) { m_SpawnInterval = interval; }
};

