#pragma once
#include "Texture2D.h"
#include <SimpleMath.h>

class Camera;
class KeyTexture
{
public:
	void Init();
	void Draw(Camera* cam, float t, const DirectX::SimpleMath::Vector3 screenPos[6]);

	//調整用
	static constexpr float kStartDelay = 0.2f;	//最初にどれだけ待つか
	static constexpr float kStagger = 0.1f;		//隣のキーが出るまでのずれ時間
	static constexpr float kRiseTime = 0.35f;	//上がり切るまでの時間
	static constexpr float kRiseDist = 120.0f;	//どれだけ下から来るか
	static constexpr float kHoldTime = 1.2f;	//上がり切ってから消え始めるまでの時間
	static constexpr float kFadeTime = 1.4f;	//消えるまで
	static constexpr float kPeakAlpha = 0.8f;	//一番濃い時の濃さ
	static constexpr float kTargetDY = -80.0f;	//判定円からどれだけ下に止めるか
	static constexpr float kSize = 70.0f;		//サイズ

private:
	Texture2D m_Keys[6];
};

