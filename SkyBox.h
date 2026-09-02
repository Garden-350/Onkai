#pragma once
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"
#include "StaticMesh.h"
#include "Texture2D.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Shader.h"
#include <array>
#include <vector>
class Camera;
class SkyBox : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	void SetTimeOfDay(float t01) { m_TimeOfDay = t01; }

private:
	StaticMesh   m_SphereMesh;   // 大きな逆向き球（内側から見る用）
	MeshRenderer m_MeshRenderer;
	Shader       m_Shader;

	Texture m_GradientTex;
	Texture m_CloudTex;

	float m_SkyboxTime = 0.0f;   // 雲のスクロール等に使う経過時間
	float m_TimeOfDay = 0.5f;    // 0=夜, 0.5=昼, 1=夕方 など好みで
	float m_Radius = 99990.08f;     // カメラのFar Planeより十分内側〜外側に

	struct SkyPSParms
	{
		float time;
		float timeOfDay;
		float padding[2]; // 16バイト境界に合わせるためのパディング
	};
};

