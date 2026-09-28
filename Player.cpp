#include "Player.h"
#include "Game.h"
#include "Input.h"
#include <cmath>

using namespace DirectX::SimpleMath;

void Player::Init()
{
	StaticMesh mesh;
	mesh.Load(
		"assets/model/Player/player.fbx",
		"assets/model/player"
	);

	if (mesh.GetSubsets().empty())
	{
		OutputDebugStringA("[Player] Failed to load player model!\n");
	}

	m_MeshRenderer.Init(mesh);
	m_Shader.Create(
		"shader/litTextureVS.hlsl",
		"shader/litTexturePS.hlsl"
	);
	m_Subsets = mesh.GetSubsets();
	m_Textures = mesh.GetTextures();
	auto materials = mesh.GetMaterials();
	for (auto& m : materials)
	{
		auto material = std::make_unique<Material>();
		material->Create(m);
		m_Materials.emplace_back(std::move(material));
	}

	m_Size = Vector3(0.05f, 0.05f, 0.05f);

	// カメラに一番近い・一番低い位置＝スケッチの「じぶん」の位置に合わせる
	m_BasePosition = Vector3(0.0f, -60.0f, -550.0f);
	m_Position = m_BasePosition;
	m_VisualX = m_BasePosition.x;
}

void Player::Update()
{
	const auto nowClock = std::chrono::steady_clock::now();
	if (!m_Started)
	{
		m_LastUpdateClock = nowClock;
		m_Started = true;
	}
	const float deltaTime = std::chrono::duration<float>(nowClock - m_LastUpdateClock).count();
	m_LastUpdateClock = nowClock;

	if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState('F') & 0x8000)
		m_Side = -1;
	else if (GetAsyncKeyState('J') & 0x8000 || GetAsyncKeyState('K') & 0x8000 || GetAsyncKeyState('L') & 0x8000)
		m_Side = 1;
	else
		m_Side = 0;

	float targetX = m_BasePosition.x + m_Side * m_MoveRange;
	float diff = targetX - m_VisualX;
	float maxStep = m_MoveSpeed * deltaTime;
	if (fabsf(diff) <= maxStep) m_VisualX = targetX;
	else m_VisualX += (diff > 0.0f ? maxStep : -maxStep);

	m_Position.x = m_VisualX;
	m_Position.y = m_BasePosition.y;
	m_Position.z = m_BasePosition.z;
}

void Player::Draw(Camera* cam)
{
	cam->SetCamera(0);
	Renderer::SetBlendState(BS_NONE);

	m_Shader.SetGPU();
	m_MeshRenderer.BeforeDraw();

	Matrix s = Matrix::CreateScale(m_Size);
	Matrix x = Matrix::CreateRotationX(m_Rotation.x);
	Matrix y = Matrix::CreateRotationY(m_Rotation.y);
	Matrix z = Matrix::CreateRotationZ(m_Rotation.z);
	Matrix t = Matrix::CreateTranslation(m_Position);
	Matrix world = s * x * y * z * t;
	Renderer::SetWorldMatrix(&world);

	for (size_t i = 0; i < m_Subsets.size(); i++)
	{
		m_Materials[m_Subsets[i].MaterialIdx]->SetGPU();
		if (m_Materials[m_Subsets[i].MaterialIdx]->isTextureEnable())
		{
			m_Textures[m_Subsets[i].MaterialIdx]->SetGPU();
		}
		m_MeshRenderer.DrawSubset(
			m_Subsets[i].IndexNum,
			m_Subsets[i].IndexBase,
			m_Subsets[i].VertexBase
		);
	}
}

void Player::Uninit()
{
}