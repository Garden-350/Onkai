#ifndef __SCENE_PARTICLE_SYSTEM_H__
#define __SCENE_PARTICLE_SYSTEM_H__

#include "SceneBase.hpp"
#include "Emitter.h"

class SceneParticleSystem : public SceneBase
{
public:
	void Init() override;
	void Uninit() override;
	void Update(float tick) override;
	void Draw() override;
private:
	float m_time = 0.0f;
	float m_speed = 1.0f;

	float m_paramChangeTimer = 0.0f;
	float m_paramChangeInterval = 0.5f;
	
	Emitter m_emitter = Emitter(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

	void RandomizeParams();
};

#endif // __SCENE_PARTICLE_SYSTEM_H___