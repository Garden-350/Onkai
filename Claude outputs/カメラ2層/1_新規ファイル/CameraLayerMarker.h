#pragma once
#include "Object.h"

// ============================================================
// CameraLayerMarker : 描画順の途中に置いて、カメラの層を切り替えるだけの Object。
//   World     ... 背景など、演出で動くカメラで描く層
//   Playfield ... 譜面・判定円など、固定カメラで描く層
//   clearDepth を true にすると、切り替え時に深度をクリアして
//   前の層（背景）が後ろの層（譜面）に割り込まないようにする。
// ============================================================
class CameraLayerMarker : public Object
{
public:
	void Setup(Camera::Layer layer, bool clearDepth) { m_Layer = layer; m_ClearDepth = clearDepth; }

	void Init() override {}
	void Update() override {}
	void Draw(Camera* cam) override;
	void Uninit() override {}

private:
	Camera::Layer m_Layer = Camera::Layer::Playfield;
	bool m_ClearDepth = false;
};
