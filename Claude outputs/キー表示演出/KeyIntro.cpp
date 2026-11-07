#include "KeyIntro.h"
#include "Renderer.h"
#include "Camera.h"
#include <algorithm>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void KeyIntro::Init()
{
	// 左のレーンから S D F J K L（弱点のキーの画像と同じもの）
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

void KeyIntro::Draw(Camera* cam, float t, const Vector3 screenPos[6])
{
	// 全部のキーが消え終わったら何もしない
	const float total = kStartDelay + kStagger * 5.0f + kRiseTime + kHoldTime + kFadeTime;
	if (t < 0.0f || t > total) return;

	Renderer::SetBlendState(BS_ALPHABLEND);
	for (int i = 0; i < 6; i++)
	{
		// このキーにとっての時間（左のキーほど早く始まる）
		float lt = t - kStartDelay - kStagger * (float)i;
		if (lt <= 0.0f) continue;

		// 上がる：最初は速く、最後はゆっくり止まる（1 - (1-k)^3）
		float k = std::min(lt / kRiseTime, 1.0f);
		float e = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);

		// 濃さ：上がりながら 透明 → 半透明。待ってから 半透明 → 透明
		float alpha = kPeakAlpha * e;
		float fadeStart = kRiseTime + kHoldTime;
		if (lt > fadeStart) alpha *= std::max(0.0f, 1.0f - (lt - fadeStart) / kFadeTime);
		if (alpha <= 0.0f) continue;

		// 位置：判定円の少し下（kTargetDY）に、下から上がってくる
		Vector3 p = screenPos[i];
		p.y += kTargetDY - kRiseDist * (1.0f - e);

		m_Keys[i].SetPosition(p);
		m_Keys[i].SetAlpha(alpha);
		m_Keys[i].Draw(cam);
	}
	Renderer::SetBlendState(BS_NONE);
	cam->SetCamera(0);	// Texture2D::Draw は 2D 用のカメラに切り替えるので、3D に戻しておく
}
