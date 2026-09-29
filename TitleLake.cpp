#include "TitleLake.h"
#include "Renderer.h"
#include "Camera.h"
#include "Application.h"
#include "Game.h"
#include <cmath>
#include <random>
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
	float Rand01()
	{
		static std::mt19937 rng(777);
		return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
	}
	//時間帯ごとの色。太陽の高さ（ｈ）で決める。間は線形に混ぜ混ぜ
	struct SkyKey
	{
		float h;			// 太陽の高さ（方向ベクトルのy）
		Vector3 zenith;		// 真上の空
		Vector3 horizon;	// 地平線の空
		Vector3 sun;		// 太陽の光（色と強さ）
		Vector3 ambient;	// 空からの光
		float stars;		// 星の量（=夜の度合い）
		float clouds;		// 雲の量
		float haze;			// 霧の濃さ
	};
	const SkyKey kSkyKeys[] =
	{
		//  h       真上の空                        地平線の空                       太陽の光                   空からの光                      星     雲     霧
		{ -0.45f, Vector3(0.004f, 0.007f, 0.020f), Vector3(0.015f, 0.025f, 0.050f), Vector3(0.0f, 0.0f, 0.0f),     Vector3(0.020f, 0.028f, 0.055f), 1.0f,  0.35f, 0.5f },	// 夜
		{ -0.15f, Vector3(0.012f, 0.020f, 0.060f), Vector3(0.070f, 0.050f, 0.090f), Vector3(0.0f, 0.0f, 0.0f),     Vector3(0.035f, 0.038f, 0.065f), 0.6f,  0.40f, 0.6f },	// 薄明
		{ -0.02f, Vector3(0.030f, 0.045f, 0.160f), Vector3(0.650f, 0.180f, 0.050f), Vector3(0.30f, 0.07f, 0.015f),  Vector3(0.070f, 0.050f, 0.060f), 0.05f, 0.45f, 0.7f },	// 日の出・日の入り
		{  0.08f, Vector3(0.060f, 0.090f, 0.300f), Vector3(0.850f, 0.300f, 0.070f), Vector3(1.50f, 0.58f, 0.16f),  Vector3(0.140f, 0.120f, 0.130f), 0.0f,  0.45f, 0.7f },	// 朝焼け・夕焼け
		{  0.30f, Vector3(0.060f, 0.200f, 0.620f), Vector3(0.420f, 0.600f, 0.800f), Vector3(2.60f, 2.40f, 2.10f),  Vector3(0.240f, 0.290f, 0.380f), 0.0f,  0.40f, 0.5f },	// 昼前・昼過ぎ
		{  1.00f, Vector3(0.050f, 0.180f, 0.600f), Vector3(0.380f, 0.560f, 0.800f), Vector3(3.00f, 2.90f, 2.70f),  Vector3(0.260f, 0.320f, 0.420f), 0.0f,  0.35f, 0.5f },	// 真昼
	};
	constexpr int kSkyKeyCount = sizeof(kSkyKeys) / sizeof(kSkyKeys[0]);
}

void TitleLake::Init()
{
	//全画面の三角形
	Renderer::CreateVertexShader(m_FullVS.GetAddressOf(), m_FullLayout.GetAddressOf(), nullptr, 0, "shader/FullscreenVS.hlsl");
	Renderer::CreatePixelShader(m_FullPS.GetAddressOf(), "shader/lakePS.hlsl");
	Renderer::CreateConstantBufferWrite(sizeof(LakeParms), m_LakeCB.GetAddressOf());

	m_Grass.Init(m_ShoreBaseZ);
	m_Particles.Init(m_ShoreBaseZ);

	m_LastClock = std::chrono::steady_clock::now();
	UpdateTimeOfDay();
	UpdateWind(0.0f);
	UpdateCamera();
}

void TitleLake::Update()
{
	// Game側の deltaTime は使わず、自分で時間を測る
	auto now = std::chrono::steady_clock::now();
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	if (dt > 0.05f) dt = 0.05f;	// 読み込み直後などに一気に進まないように
	m_Dt = dt;
	m_Time += dt;

	if (m_Diving)
	{
		m_DiveTimer += dt;
		if (!m_DiveContact && m_CamPos.y <= 0.8f)
			m_DiveContact = true;
		if (m_DiveContact)
		{
			m_ContactTimer += dt;
			if (m_ContactTimer >= m_DiveHoldSeconds) m_DiveFinished = true;
		}
	}

	UpdateTimeOfDay();
	UpdateWind(dt);
	UpdateCamera();
	m_Particles.Update(dt, m_Params);
}

void TitleLake::Draw(Camera* cam)
{
	auto* ctx = Renderer::GetDeviceContext();
	cam->SetCamera(0);

	// 時間帯・風・カメラの情報を定数バッファへ
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(ctx->Map(m_LakeCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_Params, sizeof(m_Params));
		ctx->Unmap(m_LakeCB.Get(), 0);
	}
	//空・水面・岸
	ID3D11Buffer* nullVB = nullptr;
	UINT stride = 0, offset = 0;
	ctx->IASetInputLayout(nullptr);
	ctx->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->VSSetShader(m_FullVS.Get(), nullptr, 0);
	ctx->PSSetShader(m_FullPS.Get(), nullptr, 0);
	ID3D11Buffer* cb = m_LakeCB.Get();
	ctx->PSSetConstantBuffers(6, 1, &cb);
	Renderer::SetBlendState(BS_NONE);
	Renderer::SetDepthOverlayEnable(true);
	ctx->Draw(3, 0);
	Renderer::SetDepthOverlayEnable(false);

	//草と葦
	m_Grass.Draw(cam, cb);
	//葉っぱと蛍
	m_Particles.Draw(cam, m_Params);

	Renderer::SetBlendState(BS_ALPHABLEND);
}

void TitleLake::StartDive()
{
	if (m_Diving) return;
	m_Diving = true;
	m_DiveTimer = 0.0f;
	m_DiveStart = m_CamPos;
}

float TitleLake::GetDiveProgress() const
{
	if (!m_Diving) return 0.0f;
	return Saturate(m_DiveTimer / m_DiveSeconds);
}

float TitleLake::GetDiveFade() const
{
	if (!m_DiveContact) return 0.0f;
	return Saturate(m_ContactTimer / 0.12f);	// 水に入った瞬間に一気に覆う
}

DirectX::SimpleMath::Vector3 TitleLake::GetDiveFadeColor() const
{
	float night = GetNightAmount();
	Vector3 day(0.75f, 0.92f, 1.0f);
	Vector3 dark(0.03f, 0.08f, 0.16f);
	return day + (dark - day) * night;
}

float TitleLake::GetIntroFade() const
{
	return 1.0f - SmoothStep(0.0f, 1.5f, m_Time);
}

//時間帯
void TitleLake::UpdateTimeOfDay()
{
	float phase = m_StartPhase + m_Time / m_DayLength;
	phase -= floorf(phase);

	// 太陽：左手前から昇って、正面の上を通り、右手前に沈む（湖に光の道ができる向き）
	const float theta = (phase - 0.25f) * 6.2831853f;
	Vector3 sun(-cosf(theta) * 0.35f, sinf(theta) * 0.85f, 0.65f);
	sun.Normalize();
	// 月：太陽の反対側の時刻に出る
	Vector3 moon(cosf(theta) * 0.25f, -sinf(theta) * 0.28f, 0.70f);	// 真夜中でも画面の上の方に見える高さまで
	moon.Normalize();

	// 太陽の高さで色を補間
	const float h = sun.y;
	int i = 0;
	while (i < kSkyKeyCount - 2 && h > kSkyKeys[i + 1].h) i++;
	const SkyKey& a = kSkyKeys[i];
	const SkyKey& b = kSkyKeys[i + 1];
	const float t = Saturate((h - a.h) / (b.h - a.h));
	auto L3 = [t](const Vector3& x, const Vector3& y) { return x + (y - x) * t; };
	auto L1 = [t](float x, float y) { return x + (y - x) * t; };

	const Vector3 zenith = L3(a.zenith, b.zenith);
	const Vector3 horizon = L3(a.horizon, b.horizon);
	const Vector3 sunCol = L3(a.sun, b.sun);
	const Vector3 ambient = L3(a.ambient, b.ambient);
	const float night = L1(a.stars, b.stars);

	m_Params.sunDir = Vector4(sun.x, sun.y, sun.z, SmoothStep(-0.03f, 0.03f, h));
	m_Params.sunColor = Vector4(sunCol.x, sunCol.y, sunCol.z, night);
	m_Params.moonDir = Vector4(moon.x, moon.y, moon.z, night * 0.35f * Saturate(moon.y * 4.0f));
	m_Params.skyZenith = Vector4(zenith.x, zenith.y, zenith.z, night);
	m_Params.skyHorizon = Vector4(horizon.x, horizon.y, horizon.z, L1(a.clouds, b.clouds));
	m_Params.ambientCol = Vector4(ambient.x, ambient.y, ambient.z, L1(a.haze, b.haze));
}

//風と突風
void TitleLake::UpdateWind(float dt)
{
	if (m_GustTimer < 0.0f)
	{
		m_NextGust -= dt;
		if (m_NextGust <= 0.0f)
		{
			m_GustTimer = 0.0f;
			m_GustPeak = 0.6f + 0.4f * Rand01();
		}
	}
	else
	{
		m_GustTimer += dt;
		if (m_GustTimer >= m_GustSeconds)
		{
			m_GustTimer = -1.0f;
			m_NextGust = m_GustMinInterval + (m_GustMaxInterval - m_GustMinInterval) * Rand01();
		}
	}

	if (m_GustTimer >= 0.0f)
	{
		// ぐっと強くなって、しばらく吹いて、ゆっくり収まる
		float env = SmoothStep(0.0f, 0.7f, m_GustTimer) * (1.0f - SmoothStep(m_GustSeconds * 0.55f, m_GustSeconds, m_GustTimer));
		m_Gust = env * m_GustPeak;
		m_GustFront = -900.0f + m_GustTimer * 1000.0f;	// 風上から草原を渡ってくる
	}
	else
	{
		m_Gust = 0.0f;
		m_GustFront = -100000.0f;
	}

	Vector2 w = m_WindDir;
	w.Normalize();
	m_Params.wind = Vector4(w.x, w.y, m_WindBase + m_Gust * 0.3f, m_Gust);
	m_Params.misc = Vector4(m_GustFront, m_ShoreBaseZ, GetDiveProgress(), 0.0f);
}

void TitleLake::UpdateCamera()
{
	Vector3 pos = m_EyePos + Vector3(sinf(m_Time * 0.13f) * 6.0f, sinf(m_Time * 0.21f) * 2.0f, 0.0f);
	float pitch = m_PitchDeg + sinf(m_Time * 0.17f) * 0.4f;
	float yaw = sinf(m_Time * 0.09f) * 1.2f;

	if (m_Diving)
	{
		float t = GetDiveProgress();
		float e = powf(t, 2.4f);	// だんだん加速
		Vector3 end(m_DiveStart.x, -4.0f, m_ShoreBaseZ + 230.0f);
		pos = m_DiveStart + (end - m_DiveStart) * e;
		pitch = m_PitchDeg + (m_DivePitchDeg - m_PitchDeg) * SmoothStep(0.0f, 0.8f, t);
		yaw *= 1.0f - t;
	}

	float p = DirectX::XMConvertToRadians(pitch);
	float y = DirectX::XMConvertToRadians(yaw);
	Vector3 dir(sinf(y) * cosf(p), sinf(p), cosf(y) * cosf(p));
	m_CamPos = pos;
	m_CamTarget = pos + dir * 1000.0f;
	Game::GetInstance()->GetCamera()->SetFixedPose(m_CamPos, m_CamTarget);

	// シェーダー用：Camera の LookAtLH と同じ向きの計算
	Vector3 fwd = m_CamTarget - m_CamPos;
	fwd.Normalize();
	Vector3 right = Vector3(0.0f, 1.0f, 0.0f).Cross(fwd);
	right.Normalize();
	Vector3 up = fwd.Cross(right);
	float aspect = (float)Application::GetWidth() / (float)Application::GetHeight();
	m_Params.camPos = Vector4(pos.x, pos.y, pos.z, m_Time);
	m_Params.camForward = Vector4(fwd.x, fwd.y, fwd.z, tanf(DirectX::XMConvertToRadians(45.0f) * 0.5f));
	m_Params.camRight = Vector4(right.x, right.y, right.z, aspect);
	m_Params.camUp = Vector4(up.x, up.y, up.z, m_Exposure);
}
