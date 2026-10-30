#include "SpotlightRow.h"
#include "Camera.h"
#include "Application.h"
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	constexpr float kPi = 3.14159265f;
	constexpr float kDegToRad = kPi / 180.0f;
	constexpr float kCameraFovDeg = 45.0f;	// Camera::SetCamera(0) と同じ視野角

	// 2色を交互に使う
	const Vector3 kColorA(0.35f, 0.70f, 1.00f);	// 青
	const Vector3 kColorB(1.00f, 0.00f, 0.00f);	// 赤

	constexpr float kPulseWidth = 0.12f;	// 走る光の長さ（列全体を1としたときの幅）
	constexpr float kDownbeatFlash = 0.15f;	// 拍の頭で全体が一瞬明るくなる量（0で無し）

	Vector3 Lerp3(const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; }
}

void SpotlightRow::Init()
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
	verts[0].uv = Vector2(0.0f, 0.0f);	// uv.y=0 が柱の先端(上)
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);	// uv.y=1 が柱の根本(下)
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/spotlightPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(SpotlightParams));
}

void SpotlightRow::Update()
{
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted)
	{
		m_LastClock = now;
		m_ClockStarted = true;
	}
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	if (dt > 0.1f) dt = 0.1f;	// ポーズ明けなどで大きく飛ばないように
	m_Time += dt;
}

// 画面の左下/右下の角から視線を飛ばし、床の高さ(m_BaseY)にぶつかった所を「手前の端」にする
void SpotlightRow::ComputeNearFromScreen(Camera* cam)
{
	Matrix invView = cam->GetViewMatrix().Invert();
	Vector3 right(invView._11, invView._12, invView._13);
	Vector3 up(invView._21, invView._22, invView._23);
	Vector3 forward(invView._31, invView._32, invView._33);
	Vector3 camPos(invView._41, invView._42, invView._43);

	float tanHalf = tanf(kCameraFovDeg * 0.5f * kDegToRad);
	float aspect = (float)Application::GetWidth() / (float)Application::GetHeight();

	bool ok = true;
	Vector3 hits[2];
	const float ndcX[2] = { -m_ScreenEdgeX, m_ScreenEdgeX };
	for (int s = 0; s < 2; s++)
	{
		Vector3 dir = forward + right * (ndcX[s] * tanHalf * aspect) + up * (m_ScreenEdgeY * tanHalf);
		if (dir.y > -0.0001f) { ok = false; break; }	// 床に向かっていない（カメラが上を向いている）
		float t = (m_BaseY - camPos.y) / dir.y;
		if (t <= 0.0f) { ok = false; break; }
		hits[s] = camPos + dir * t;
	}

	if (ok)
	{
		m_NearLeft = hits[0];
		m_NearRight = hits[1];
	}
	m_NearComputed = true;	// 失敗した場合は予備値のまま使う
}

void SpotlightRow::DrawQuad(const Matrix& world, const SpotlightParams& params)
{
	Matrix w = world;
	Renderer::SetWorldMatrix(&w);
	m_Shader.UpdatePSConstantBuffer(&params, sizeof(params));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}

void SpotlightRow::Draw(Camera* cam)
{
	cam->SetCamera(0);
	if (!m_NearComputed) ComputeNearFromScreen(cam);

	const Matrix invView = cam->GetViewMatrix().Invert();
	const Vector3 camPos(invView._41, invView._42, invView._43);
	const Vector3 camRight(invView._11, invView._12, invView._13);
	const Vector3 camUp(invView._21, invView._22, invView._23);

	Renderer::SetBlendState(BS_ADDITIVE);
	Renderer::SetDepthEnable(false);	// 深度は書き込まない（後から描くノーツ等を邪魔しない）
	Renderer::SetCullMode(CULL_NONE);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//拍の位置
	float beatPos = (m_HasBeat && m_Bpm > 0.0f) ? m_MusicTime * m_Bpm / 120.0f : m_Time * 2.0f;
	float beatFrac = beatPos - floorf(beatPos);
	float pulseHead = beatFrac * 1.4f - 0.2f;			// 走る光の先頭（少し手前から出て、少し奥まで抜ける）
	float flash = expf(-beatFrac * 8.0f) * kDownbeatFlash;

	const Vector3 farLeft(m_FarTarget.x - m_FarHalfWidth, m_BaseY, m_FarTarget.z);
	const Vector3 farRight(m_FarTarget.x + m_FarHalfWidth, m_BaseY, m_FarTarget.z);

	const int count = (m_LightsPerSide < 2) ? 2 : m_LightsPerSide;
	for (int side = 0; side < 2; side++)
	{
		const float outward = (side == 0) ? -1.0f : 1.0f;	// 左列は-x側、右列は+x側へ傾ける
		const Vector3& nearP = (side == 0) ? m_NearLeft : m_NearRight;
		const Vector3& farP = (side == 0) ? farLeft : farRight;

		for (int i = 0; i < count; i++)
		{
			const float t = (float)i / (float)(count - 1);	// 0=手前(画面の端) 〜 1=奥(敵)
			const Vector3 base = Lerp3(nearP, farP, t);

			// 明るさ：普段の明るさ + 走る光 + 拍頭のフラッシュ
			float d = (t - pulseHead) / kPulseWidth;
			float pulse = expf(-d * d);
			float intensity = (m_BaseIntensity + m_PulseIntensity * pulse + flash)
				* (0.35f + 0.65f * m_Hype);

			const Vector3 color = (i % 2 == 0) ? kColorA : kColorB;

			// 向き：「上」を決めてから、外側へ傾け、ゆらゆら揺らす
			//   m_ScreenUpBlend=1 のとき「画面の上」方向（カメラが見下ろしていても画面上でまっすぐ縦に伸びる）
			//   m_ScreenUpBlend=0 のとき「ワールドの真上」方向（見下ろしカメラだと手前に倒れて見える）
			Vector3 upDir = Vector3::Up * (1.0f - m_ScreenUpBlend) + camUp * m_ScreenUpBlend;
			upDir.Normalize();
			float sway = sinf(m_Time * 0.7f + i * 0.6f + side * 1.3f) * m_SwayDeg * (0.5f + 0.5f * m_Hype);
			float lean = (m_LeanDeg + sway) * kDegToRad;
			Vector3 axis = upDir * cosf(lean) + camRight * (outward * sinf(lean));
			axis.Normalize();

			//光の柱：軸まわりだけカメラの方へ向ける
			{
				Vector3 mid = base + axis * (m_BeamLength * 0.5f);
				Vector3 toCam = camPos - mid;
				Vector3 sideVec = axis.Cross(toCam);
				if (sideVec.LengthSquared() < 0.0001f) sideVec = Vector3::Right;
				sideVec.Normalize();
				Vector3 normal = sideVec.Cross(axis);
				normal.Normalize();

				Matrix world(
					sideVec.x * m_TopWidth, sideVec.y * m_TopWidth, sideVec.z * m_TopWidth, 0.0f,
					axis.x * m_BeamLength, axis.y * m_BeamLength, axis.z * m_BeamLength, 0.0f,
					normal.x, normal.y, normal.z, 0.0f,
					mid.x, mid.y, mid.z, 1.0f);

				SpotlightParams p = {};
				p.color = Vector4(color.x, color.y, color.z, intensity);
				p.mode = 0.0f;
				p.baseWidthRatio = m_BaseWidth / m_TopWidth;
				DrawQuad(world, p);
			}

			//根本のランプ：完全にカメラの方を向く板
			{
				float size = m_LampSize * (0.6f + 0.4f * intensity);
				Vector3 normal = camPos - base;
				normal.Normalize();
				Matrix world(
					camRight.x * size, camRight.y * size, camRight.z * size, 0.0f,
					camUp.x * size, camUp.y * size, camUp.z * size, 0.0f,
					normal.x, normal.y, normal.z, 0.0f,
					base.x, base.y, base.z, 1.0f);

				SpotlightParams p = {};
				p.color = Vector4(color.x, color.y, color.z, intensity);
				p.mode = 1;
				DrawQuad(world, p);
			}
		}
	}

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_NONE);
}
