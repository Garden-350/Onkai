#include "JudgeLineGuide.h"
#include "Camera.h"
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	const Vector3 kStrikeColor(0.75f, 0.90f, 1.00f);	// ストライクラインの色
	const Vector3 kBeatLineColor(0.70f, 0.85f, 1.00f);	// 拍の線の色

	float Saturate(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
	float SmoothStep(float e0, float e1, float x)
	{
		float t = Saturate((x - e0) / (e1 - e0));
		return t * t * (3.0f - 2.0f * t);
	}
}

void JudgeLineGuide::Init()
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
	verts[0].uv = Vector2(0.0f, 0.0f);	// uv.y=0 が +axisV 側（トラックなら奥）
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);	// uv.y=1 が -axisV 側（トラックなら判定円側）
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/judgeGuidePS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(GuideParams));
}

void JudgeLineGuide::DrawBasis(const Vector3& center, const Vector3& axisU, const Vector3& axisV, const GuideParams& params)
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

void JudgeLineGuide::DrawRibbon(const Vector3& from, const Vector3& to, float width, const Vector3& camPos, const GuideParams& params)
{
	Vector3 axisV = to - from;
	if (axisV.LengthSquared() < 0.0001f) return;
	Vector3 mid = (from + to) * 0.5f;

	Vector3 side = axisV.Cross(camPos - mid);	// 線の軸まわりだけカメラの方へ向ける
	if (side.LengthSquared() < 0.000001f) return;
	side.Normalize();

	DrawBasis(mid, side * width, axisV, params);
}

void JudgeLineGuide::Draw(Camera* cam, const Frame& f)
{
	cam->SetCamera(0);
	const Matrix invView = cam->GetViewMatrix().Invert();
	const Vector3 camPos(invView._41, invView._42, invView._43);

	Renderer::SetBlendState(BS_ADDITIVE);
	Renderer::SetDepthEnable(false);	// 深度は書き込まない（後から描くノーツを邪魔しない）
	Renderer::SetCullMode(CULL_NONE);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const int n = kLaneCount;

	// ── レーンごとの「横方向」と「隣のレーンとの間隔」──
	Vector3 lateral[kLaneCount];
	float spacing[kLaneCount];
	for (int i = 0; i < n; i++)
	{
		int prev = (i > 0) ? i - 1 : i;
		int next = (i < n - 1) ? i + 1 : i;
		Vector3 d = f.judgePos[next] - f.judgePos[prev];
		float steps = (float)(next - prev);
		float len = d.Length();
		if (len < 0.001f || steps < 1.0f)
		{
			lateral[i] = Vector3::Right;
			spacing[i] = 90.0f;
		}
		else
		{
			lateral[i] = d / len;
			spacing[i] = len / steps;
		}
	}

	// ── 拍の位置 ──
	const bool hasBeat = (f.bpm > 0.0f);
	const float secPerBeat = hasBeat ? 60.0f / f.bpm : 0.5f;
	const float beatPos = f.musicTime / secPerBeat;
	const float beatFrac = beatPos - floorf(beatPos);

	// ============================================================
	// 1) レーンのトラック（床の帯）
	// ============================================================
	for (int i = 0; i < n; i++)
	{
		const Vector3& along = f.trackOffset[i];
		if (along.LengthSquared() < 1.0f) continue;

		Vector3 start = f.judgePos[i] - along * m_TrackBehind;	// 判定円より少し手前から
		Vector3 end = f.judgePos[i] + along;					// ノーツが出てくる奥の端まで
		Vector3 center = (start + end) * 0.5f;
		Vector3 axisV = end - start;
		Vector3 axisU = lateral[i] * (spacing[i] * m_TrackWidthRatio);

		float glow = Saturate(f.laneGlow[i]);
		GuideParams p = {};
		p.color = Vector4(f.laneColor[i].x, f.laneColor[i].y, f.laneColor[i].z, m_TrackIntensity * (1.0f + 0.8f * glow));
		p.mode = 1.0f;
		p.param1 = m_TrackBehind / (1.0f + m_TrackBehind);	// 帯の中で判定円がある位置(根本からの割合)
		DrawBasis(center, axisU, axisV, p);
	}

	// ============================================================
	// 2) 拍の線（奥から判定ラインへ流れてくる）
	// ============================================================
	if (hasBeat && f.approachSeconds > 0.01f)
	{
		long long k0 = (long long)ceilf(f.musicTime / secPerBeat);
		long long k1 = (long long)floorf((f.musicTime + f.approachSeconds) / secPerBeat);
		if (k1 - k0 > 64) k1 = k0 + 64;	// 念のための上限

		for (long long k = k0; k <= k1; k++)
		{
			float s = ((float)k * secPerBeat - f.musicTime) / f.approachSeconds;	// 0=判定ライン, 1=奥の端
			if (s < 0.0f || s > 1.0f) continue;

			bool isBar = false;
			if (f.beatsPerBar > 0)
			{
				long long m = k % f.beatsPerBar;
				if (m < 0) m += f.beatsPerBar;
				isBar = (m == 0);
			}

			float fade = (1.0f - SmoothStep(0.7f, 1.0f, s)) * (0.4f + 0.6f * (1.0f - s));	// 奥はうっすら、近いほどはっきり
			float intensity = (isBar ? m_BarLineIntensity : m_BeatLineIntensity) * fade;
			if (intensity <= 0.001f) continue;
			float width = isBar ? m_BarLineWidth : m_BeatLineWidth;

			Vector3 pts[kLaneCount + 2];
			for (int i = 0; i < n; i++) pts[i + 1] = f.judgePos[i] + f.trackOffset[i] * s;
			pts[0] = pts[1] - (pts[2] - pts[1]) * 0.5f;				// 両端はレーン半分だけはみ出させる
			pts[n + 1] = pts[n] + (pts[n] - pts[n - 1]) * 0.5f;

			GuideParams p = {};
			p.color = Vector4(kBeatLineColor.x, kBeatLineColor.y, kBeatLineColor.z, intensity);
			p.mode = 0.0f;
			for (int i = 0; i < n + 1; i++) DrawRibbon(pts[i], pts[i + 1], width, camPos, p);
		}
	}

	// ============================================================
	// 3) ストライクライン（6つの判定円をつなぐ光るライン）
	// ============================================================
	{
		Vector3 pts[kLaneCount + 2];
		for (int i = 0; i < n; i++) pts[i + 1] = f.judgePos[i];
		pts[0] = pts[1] - (pts[2] - pts[1]) * 0.5f;
		pts[n + 1] = pts[n] + (pts[n] - pts[n - 1]) * 0.5f;

		float pulse = hasBeat ? expf(-beatFrac * 6.0f) * m_StrikeBeatPulse : 0.0f;	// 拍の頭で少し明るく
		float strength = m_StrikeIntensity + pulse;

		GuideParams glowP = {};
		glowP.color = Vector4(kStrikeColor.x, kStrikeColor.y, kStrikeColor.z, strength * 0.35f);
		glowP.mode = 0.0f;
		GuideParams coreP = glowP;
		coreP.color.w = strength;

		for (int i = 0; i < n + 1; i++)
		{
			DrawRibbon(pts[i], pts[i + 1], m_StrikeGlowWidth, camPos, glowP);
			DrawRibbon(pts[i], pts[i + 1], m_StrikeCoreWidth, camPos, coreP);
		}
	}

	// ============================================================
	// 4) 判定円のパッド（押すと光る）
	// ============================================================
	for (int i = 0; i < n; i++)
	{
		Vector3 alongDir = f.trackOffset[i];
		if (alongDir.LengthSquared() < 1.0f) alongDir = Vector3(0.0f, 0.3f, 1.0f);	// トラック情報が無いときの仮の向き
		alongDir.Normalize();

		float glow = Saturate(f.laneGlow[i]);
		GuideParams p = {};
		p.color = Vector4(f.laneColor[i].x, f.laneColor[i].y, f.laneColor[i].z,
			m_PadIntensity + (m_PadPressIntensity - m_PadIntensity) * glow);
		p.mode = 2.0f;
		p.param1 = glow;
		// トラックと同じ面に寝かせて置く（奥行きに合わせてつぶれて見える）
		DrawBasis(f.judgePos[i], lateral[i] * m_PadSize, alongDir * m_PadSize, p);
	}

	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_NONE);
}
