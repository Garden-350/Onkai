#include "KeyTexture.h"
#include "Renderer.h"
#include "Camera.h"
#include <algorithm>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void KeyTexture::Init()
{
	//左のレーンから出る
	const char* paths[6] = 
	{
		"assets/texture/S.png", "assets/texture/D.png", "assets/texture/F.png",
		"assets/texture/J.png", "assets/texture/K.png", "assets/texture/L.png",
	};
	for (int i = 0; i < 6; i++)
	{
		m_Keys[i].Init();
		m_Keys[i].SetTexture(paths[i]);
		m_Keys[i].SetScale(kSize, kSize, 1.0f);
	}
}

void KeyTexture::Draw(Camera* cam, float t, const DirectX::SimpleMath::Vector3 screenPos[6])
{
	//全部のキーが消えたら何もしない
	const float total = kStartDelay + kStagger * 5.0f + kRiseTime + kHoldTime + kFadeTime;
	if (t < 0.0f || t > total) return;

	Renderer::SetBlendState(BS_ALPHABLEND);
	for (int i = 0; i < 6; i++)
	{
		//見てるキーの時間（左ほど速い）
		float it = t - kStartDelay - kStagger * (float)i;
		if (it <= 0.0f) continue;

		//上がる
		float k = std::min(it / kRiseTime, 1.0f);
		float s = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);

		//濃さ
		float alpha = kPeakAlpha * s;
		float fadeStart = kRiseTime + kHoldTime;
		if (it > fadeStart) alpha *= std::max(0.0f, 1.0f - (it - fadeStart) / kFadeTime);
		if (alpha <= 0.0f) continue;

		//位置
		Vector3 p = screenPos[i];
		p.y += kTargetDY - kRiseDist * (1.0f - s);

		m_Keys[i].SetPosition(p);
		m_Keys[i].SetAlpha(alpha);
		m_Keys[i].Draw(cam);
	}
	Renderer::SetBlendState(BS_NONE);
	cam->SetCamera(0);
}
