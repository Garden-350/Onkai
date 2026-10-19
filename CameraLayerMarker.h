#pragma once
#include "Object.h"

//•`‰æ‡‚Ì“r’†‚É’u‚¢‚ÄƒJƒƒ‰‚Ì‘w‚ğØ‚è‘Ö‚¦‚é‚¾‚¯‚Ì‚à‚Ì
class CameraLayerMarker : public Object
{
private:
	Camera::Layer m_Layer = Camera::Layer::Playfield;
	bool m_ClearDepth = false;	//true‚È‚çØ‚è‘Ö‚¦‚É[“x‚ğƒNƒŠƒAi”wŒi‚ª•ˆ–Ê‚ÉŠ„‚è‚Ü‚È‚¢‚æ‚¤‚Éj
public:
	void SetUp(Camera::Layer layer, bool clearDepth)
	{
		m_Layer = layer;
		m_ClearDepth = clearDepth;
	}
	void Init() override {}
	void Update() override{}
	void Draw(Camera* cam)override;
	void Uninit() override{}
};

