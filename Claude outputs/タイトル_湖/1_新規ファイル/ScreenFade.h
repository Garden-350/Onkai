#pragma once
// ============================================================
// ScreenFade : 画面全体を1色で覆うだけのオブジェクト（フェードイン・フェードアウト用）。
//   SetColor で色、SetAlpha で濃さ(0=見えない, 1=塗りつぶし)を決める。
//   ※シーンに AddObject する順番は「一番最後」にすること（一番手前に描くため）。
// ============================================================
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include <SimpleMath.h>

class ScreenFade : public Object
{
public:
	void Init() override;
	void Update() override {}
	void Draw(Camera* cam) override;
	void Uninit() override {}

	void SetColor(const DirectX::SimpleMath::Vector3& color) { m_FadeColor = color; }
	void SetAlpha(float alpha) { m_Alpha = alpha; }
	float GetAlpha() const { return m_Alpha; }

private:
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	DirectX::SimpleMath::Vector3 m_FadeColor = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	float m_Alpha = 0.0f;
};
