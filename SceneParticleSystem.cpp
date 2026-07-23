#include "SceneParticleSystem.h"
#include "Geometory.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void SceneParticleSystem::Init()
{
	Texture* p_tex = CreateObj<Texture>("ParticleTex");
	p_tex->Create("Assets/Texture/sakura2.png");
	m_emitter.SetTexture(p_tex);

	m_emitter.color_start = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // 純白(眩しい)
	m_emitter.color_end = DirectX::XMFLOAT4(1.0f, 0.9f, 0.4f, 0.0f); // 黄色寄りに消える
	m_emitter.colorChangeStartRate = 0.3f; // すぐ色が変わり始める(長く白を保たない)
	m_emitter.size_start = 0.3f;
	m_emitter.size_end = 0.02f;

	m_emitter.deg_min = -4.0f;
	m_emitter.deg_max = 40.0f;

	m_emitter.gravity = 1.0f;

	m_emitter.lifeTime_min = 0.3f;
	m_emitter.lifeTime_max = 0.8f;

	// 速度:とにかく速い、かつ個体差を大きく(バラつきで鋭さを演出)
	m_emitter.SetVector(15.0f, 35.0f, 15.0f, 35.0f);

	// 生成数:一瞬に大量発生
	m_emitter.SetGeneratNum(200, 600);

	m_emitter.SetGeneratTime(0.5f);
	m_paramChangeInterval = 0.5f;

	RandomizeParams();
}
void SceneParticleSystem::Uninit()
{

}
void SceneParticleSystem::Update(float tick)
{
	m_time += tick;
	m_paramChangeTimer += tick;
	if (m_paramChangeTimer >= m_paramChangeInterval)
	{
		m_paramChangeTimer -= m_paramChangeInterval;
		RandomizeParams();
	}
	
	m_emitter.Update(tick);
}
void SceneParticleSystem::Draw()
{
	m_emitter.Drow();
}

void SceneParticleSystem::RandomizeParams()
{
	static std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> distAngleCenter(0.0f, 240.0f);
	//今回は剣のぶつかりイメージなので0.0
	std::uniform_real_distribution<float> distAngleWidth(0.0f, 0.0f);		//ここを360にしたら花火になる
	std::uniform_real_distribution<float> distSizeStart(0.8f, 2.0f);
	std::uniform_real_distribution<float> distSizeEnd(0.05f, 0.3f);
	std::uniform_real_distribution<float> distColorR(0.1f, 1.0f);
	std::uniform_real_distribution<float> distColorG(0.1f, 1.0f);
	std::uniform_real_distribution<float> distColorB(0.1f, 1.0f);

	//角度をランダムな中心角度幅で再設定
	float centerDeg = distAngleCenter(rng);
	float width = distAngleWidth(rng);
	m_emitter.deg_min = centerDeg - width;
	m_emitter.deg_max = centerDeg + width;

	float alpha_s = m_emitter.color_start.w;
	float alpha_e = m_emitter.color_end.w;
	m_emitter.color_start = DirectX::XMFLOAT4(distColorR(rng), distColorG(rng), distColorB(rng), alpha_s);
	m_emitter.color_end = DirectX::XMFLOAT4(distColorR(rng), distColorG(rng), distColorB(rng), alpha_e);

	//サイズのランダム再設定
	m_emitter.size_start = distSizeStart(rng);
	m_emitter.size_end = distSizeEnd(rng);
}
