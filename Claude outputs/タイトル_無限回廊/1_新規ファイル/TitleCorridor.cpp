#include "TitleCorridor.h"
#include "Camera.h"
#include "Game.h"
#include <cmath>
#include <algorithm>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	float Saturate(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
	float SmoothStep(float e0, float e1, float x)
	{
		float t = Saturate((x - e0) / (e1 - e0));
		return t * t * (3.0f - 2.0f * t);
	}
	// 最初に少しだけマイナスへ振れてから一気に1へ向かう（扉が「ためて」から迫ってくる）
	float EaseInBack(float t, float overshoot)
	{
		float c3 = overshoot + 1.0f;
		return c3 * t * t * t - overshoot * t * t;
	}
}

void TitleCorridor::Init()
{
	std::vector<VERTEX_3D> verts(4);
	verts[0].position = Vector3(-0.5f, 0.5f, 0.0f);
	verts[1].position = Vector3(0.5f, 0.5f, 0.0f);
	verts[2].position = Vector3(-0.5f, -0.5f, 0.0f);
	verts[3].position = Vector3(0.5f, -0.5f, 0.0f);
	for (int i = 0; i < 4; i++)
	{
		verts[i].normal = Vector3(0.0f, 0.0f, -1.0f);
		verts[i].color = Color(1.0f, 1.0f, 1.0f, 1.0f);
	}
	verts[0].uv = Vector2(0.0f, 0.0f);	// uv.y=0 が +axisV 側（床・壁なら奥、扉なら上）
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/titleCorridorPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(CorridorParams));

	m_DoorTexture.Load("assets/texture/title_exit_door.png");

	m_DoorZ = m_DoorDistance;
	m_LastClock = std::chrono::steady_clock::now();
	UpdateCamera();
}

void TitleCorridor::StartExit()
{
	if (m_Exiting) return;
	m_Exiting = true;
	m_ExitTimer = 0.0f;
}

float TitleCorridor::GetExitProgress() const
{
	if (!m_Exiting) return 0.0f;
	return Saturate(m_ExitTimer / m_ExitSeconds);
}

float TitleCorridor::GetIntroFade() const
{
	return 1.0f - SmoothStep(0.0f, m_IntroSeconds, m_Time);
}

float TitleCorridor::GetExitWhite() const
{
	return SmoothStep(0.72f, 1.0f, GetExitProgress());
}

void TitleCorridor::Update()
{
	// Game側の deltaTime は使わず、自分で時間を測る（Stage1Scene と同じやり方）
	auto now = std::chrono::steady_clock::now();
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	if (dt > 0.05f) dt = 0.05f;	// 読み込み直後などに一気に進まないように
	m_Time += dt;

	const float prevDoorZ = m_DoorZ;
	if (m_Exiting)
	{
		m_ExitTimer += dt;
		float t = Saturate(m_ExitTimer / m_ExitSeconds);
		float e = EaseInBack(t, m_ExitAnticipation);
		m_DoorZ = m_DoorDistance + (m_ExitDoorZ - m_DoorDistance) * e;

		if (m_ExitTimer >= m_ExitSeconds + m_ExitHoldSeconds)
			m_ExitFinished = true;
	}

	// 歩き続ける。扉が近づいたぶんも線を流す（扉と床が同じ速さで迫ってくるように）
	m_Scroll += m_WalkSpeed * dt + (prevDoorZ - m_DoorZ);

	UpdateCamera();
}

void TitleCorridor::UpdateCamera()
{
	const float exitT = GetExitProgress();

	// 1マス歩くごとに1歩。出口演出中は揺れを止める（速くなりすぎてガタつくので）
	const float step = m_Scroll / m_Cell * 3.14159265f;
	const float bob = m_BobAmount * (1.0f - exitT);
	Vector3 pos(
		sinf(step) * bob * 0.5f,						// 左右にわずかに揺れる
		m_EyeHeight + (1.0f - cosf(step * 2.0f)) * 0.5f * bob,	// 1歩ごとに上下
		0.0f);

	// 出口に向かうときは見下ろすのをやめて、扉の正面を向く
	float lookDown = m_LookDownDeg * (1.0f - SmoothStep(0.0f, 0.6f, exitT));
	float rad = DirectX::XMConvertToRadians(lookDown);
	Vector3 target = pos + Vector3(0.0f, -tanf(rad) * 1000.0f, 1000.0f);

	Game::GetInstance()->GetCamera()->SetFixedPose(pos, target);
}

void TitleCorridor::DrawBasis(const Vector3& center, const Vector3& axisU, const Vector3& axisV, const CorridorParams& params)
{
	Vector3 normal = axisU.Cross(axisV);
	if (normal.LengthSquared() < 0.000001f) return;
	normal.Normalize();

	Matrix world(
		axisU.x, axisU.y, axisU.z, 0.0f,
		axisV.x, axisV.y, axisV.z, 0.0f,
		normal.x, normal.y, normal.z, 0.0f,
		center.x, center.y, center.z, 1.0f);
	Renderer::SetWorldMatrix(&world);
	m_Shader.UpdatePSConstantBuffer(&params, sizeof(params));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}

void TitleCorridor::Draw(Camera* cam)
{
	cam->SetCamera(0);
	Renderer::SetBlendState(BS_NONE);	// 回廊は不透明
	Renderer::SetDepthEnable(true);
	Renderer::SetCullMode(CULL_NONE);	// 板の裏表を気にしなくていいように
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// ── 演出の値 ──
	const float exitT = GetExitProgress();
	const float open = SmoothStep(0.25f, 0.9f, exitT);								// 扉から光があふれる量
	const float kick = m_Exiting ? expf(-m_ExitTimer * 5.0f) * 1.5f : 0.0f;			// Enterの瞬間に線がパッと明るく
	const float breathe = 0.85f + 0.15f * sinf(m_Time * 1.7f);						// 扉の光がゆっくり呼吸する
	const float doorOn = SmoothStep(0.6f, m_IntroSeconds + 0.9f, m_Time);			// 起動時、扉は最後に灯る
	const float reveal = 250.0f + SmoothStep(0.1f, m_IntroSeconds + 0.6f, m_Time) * (m_DoorDistance + 150.0f); // 線が手前から奥へ灯っていく

	CorridorParams base = {};
	base.lineColor = Vector4(m_LineColor.x, m_LineColor.y, m_LineColor.z, m_LineIntensity * (1.0f + kick));
	base.fillColor = Vector4(m_FillColor.x, m_FillColor.y, m_FillColor.z, 1.0f);
	base.fogColor = Vector4(m_FogColor.x, m_FogColor.y, m_FogColor.z, m_FogMax * (1.0f - open * 0.7f));
	base.doorColor = Vector4(m_DoorLightColor.x, m_DoorLightColor.y, m_DoorLightColor.z,
		m_DoorLightIntensity * breathe * doorOn * (1.0f + open * 2.0f));
	base.scroll = m_Scroll;
	base.cell = m_Cell;
	base.lineWidth = m_LineWidth;
	base.zDoor = m_DoorZ;
	base.fogStart = m_FogStart;
	base.fogEnd = m_FogEnd;
	base.doorW = m_DoorWidth;
	base.doorH = m_DoorHeight;
	base.openAmount = open;
	base.revealDist = reveal;

	const float W = m_Width;
	const float H = m_Height;
	const float zNear = m_NearZ;
	const float len = std::max(m_DoorZ - zNear, 1.0f);
	const float midZ = (zNear + m_DoorZ) * 0.5f;

	// ── 床・天井・左右の壁（線が手前へ流れる）──
	CorridorParams p = base;
	p.mode = 0.0f;
	p.zNear = zNear;
	p.sizeV = len;

	p.sizeU = W;
	DrawBasis(Vector3(0.0f, 0.0f, midZ), Vector3(W, 0.0f, 0.0f), Vector3(0.0f, 0.0f, len), p);	// 床
	DrawBasis(Vector3(0.0f, H, midZ), Vector3(W, 0.0f, 0.0f), Vector3(0.0f, 0.0f, len), p);		// 天井

	p.sizeU = H;
	DrawBasis(Vector3(-W * 0.5f, H * 0.5f, midZ), Vector3(0.0f, H, 0.0f), Vector3(0.0f, 0.0f, len), p);	// 左の壁
	DrawBasis(Vector3(W * 0.5f, H * 0.5f, midZ), Vector3(0.0f, H, 0.0f), Vector3(0.0f, 0.0f, len), p);	// 右の壁

	// ── 奥の壁（扉のまわりに光がにじむ）──
	p.mode = 1.0f;
	p.zNear = m_DoorZ;
	p.sizeU = W;
	p.sizeV = H;
	DrawBasis(Vector3(0.0f, H * 0.5f, m_DoorZ), Vector3(W, 0.0f, 0.0f), Vector3(0.0f, H, 0.0f), p);

	// ── 出口の扉（奥の壁のほんの少し手前）──
	m_DoorTexture.SetGPU(0);
	p.mode = 2.0f;
	p.sizeU = m_DoorWidth;
	p.sizeV = m_DoorHeight;
	DrawBasis(Vector3(0.0f, m_DoorHeight * 0.5f, m_DoorZ - 1.0f),
		Vector3(m_DoorWidth, 0.0f, 0.0f), Vector3(0.0f, m_DoorHeight, 0.0f), p);

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetBlendState(BS_ALPHABLEND);
}
