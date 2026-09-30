#pragma once
#include "Object.h"
#include "Texture2D.h"
#include "ScreenFade.h"

class PauseOverlay : public Object
{
public:
	void Init() override;
	void Update() override {};
	void Draw(Camera* cam) override;
	void Uninit() override {};
private:
	ScreenFade m_Fade;
	Texture2D m_TextRetry;
	Texture2D m_TextGo;
};

