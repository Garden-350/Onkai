#include "SkyBox.h"
#include "Game.h"
#include "Renderer.h"
#include "Camera.h"
#include "Application.h"

using namespace DirectX::SimpleMath;

void SkyBox::Init()
{
	m_SphereMesh.Load("assets/model/SkyBlock/skybox3.fbx", "assets/model/skybox");
	m_MeshRenderer.Init(m_SphereMesh);

	m_Shader.Create("shader/skyVS.hlsl", "shader/skyPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(SkyPSParms));

	if (!m_GradientTex.Load("assets/texture/skyboxTexture_3.png"))
		OutputDebugStringA("[SkyboxObject] gradient texture load failed\n");

	if (!m_CloudTex.Load("assets/texture/skyboxKumo_2.png"))
		OutputDebugStringA("[SkyboxObject] cloud texture load failed\n");
}

void SkyBox::Update()
{
	m_SkyboxTime += 1.0f / 60.0f;
	//m_Position = Game::GetInstance()->GetCamera()->GetPosition();
}

void SkyBox::Draw(Camera* cam)
{
	using namespace DirectX::SimpleMath;

	cam->SetCamera(0);

	Vector3 camPos = cam->GetPosition();
	Matrix world = Matrix::CreateScale(m_Radius) * Matrix::CreateTranslation(camPos);

	Renderer::SetDepthEnable(false);
	Renderer::SetCullMode(CULL_FRONT);

	Renderer::SetWorldMatrix(&world);
	m_Shader.SetGPU();

	SkyPSParms parms{};
	parms.time = m_SkyboxTime;
	parms.timeOfDay = m_TimeOfDay;
	m_Shader.UpdatePSConstantBuffer(&parms, sizeof(parms));

	// ── 追加：テクスチャをレジスタt0, t1にセット ──
	m_GradientTex.SetGPU(0); // t0
	m_CloudTex.SetGPU(1);    // t1

	m_MeshRenderer.BeforeDraw();
	for (auto& subset : m_SphereMesh.GetSubsets())
	{
		m_MeshRenderer.DrawSubset(subset.IndexNum, subset.IndexBase, subset.VertexBase);
	}

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthEnable(true);
}

void SkyBox::Uninit()
{
}
