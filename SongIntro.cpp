#include "SongIntro.h"
#include "Camera.h"
#include "Renderer.h"
#include "Application.h"
#include <filesystem>
#include <algorithm>
#undef min
#undef max

using namespace DirectX::SimpleMath;
namespace fs = std::filesystem;

void SongIntro::Init()
{
	//帯を描く四角　色はシェーダーに
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
	verts[0].uv = Vector2(0.0f, 0.0f);
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_RectVB.Create(verts);
	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_RectIB.Create(idx);

	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/screenFadePS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(Vector4));

	m_Jacket.Init();
	m_Name.Init();
}

void SongIntro::Update()
{
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted) { m_LastClock = now; m_ClockStarted = true; }
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	m_Time += std::min(dt, 0.1f);
}

void SongIntro::Draw(Camera* cam)
{
	float leave = Clamp01((m_Time - m_LeaveTime) / kLeaveDuration);	//消える進み具合 0→1
	if (leave >= 1.0f) return;	//消え終わったら何も描かない

	float open = EaseOut(m_Time / kOpenTime);					//帯が開く 0→1
	float jk = EaseOut(m_Time / kSlideTime);					//ジャケットが入る 0→1
	float nm = EaseOut((m_Time - kNameDelay) / kSlideTime);	//曲名が入る 0→1
	float out = leave * leave;	//消える時は、だんだん速く
	float fade = 1.0f - out;
	float cy = kCenterY + kLeaveRise * out;	//消える時に少し上へ

	cam->SetCamera(1);	//2D（画面の真ん中が0,0、1単位=1ピクセル）
	Renderer::SetBlendState(BS_ALPHABLEND);
	Renderer::SetDepthOverlayEnable(true);	//何があっても一番手前
	Renderer::SetCullMode(CULL_NONE);

	//帯：開く時は上下に広がり、消える時は少し細くなる
	float W = (float)Application::GetWidth();
	float bandH = kBandH * open * (1.0f - 0.6f * out);
	DrawRect(0.0f, cy, W, bandH, Vector4(0.0f, 0.0f, 0.0f, kBandAlpha * fade));
	//帯の上下の細い光の線（真ん中から左右へ伸びる）
	const Vector4 lineColor(0.45f, 0.75f, 1.0f, 0.9f * fade);
	DrawRect(0.0f, cy + bandH * 0.5f, W * open, 3.0f, lineColor);
	DrawRect(0.0f, cy - bandH * 0.5f, W * open, 3.0f, lineColor);
	Renderer::SetCullMode(CULL_BACK);

	//ジャケット：左からすべり込む
	m_Jacket.SetPosition(kJacketX - kSlideDist * (1.0f - jk), cy, 0.0f);
	m_Jacket.SetScale(kJacketSize, kJacketSize, 1.0f);
	m_Jacket.SetAlpha(jk * fade);
	m_Jacket.Draw(cam);

	//曲名：右からすべり込む
	m_Name.SetPosition(kNameX + kSlideDist * (1.0f - nm), cy, 0.0f);
	m_Name.SetScale(kNameW, kNameH, 1.0f);
	m_Name.SetAlpha(nm * fade);
	m_Name.Draw(cam);

	Renderer::SetDepthOverlayEnable(false);
	Renderer::SetBlendState(BS_NONE);
	cam->SetCamera(0);	//後の3D描画のために戻す
}

void SongIntro::Uninit()
{
}

void SongIntro::SetSong(const std::string& name)
{
	//ジャケット：png,jpg,jpegの順に探してなかったらdefault.png
	std::string jacket = "assets/jacket/default.png";
	for (const char* ext : { ".png",".jpg",".jpeg" })
	{
		std::string path = "assets/jacket/" + name + ext;
		if (fs::exists(path)) { jacket = path; break; }
	}
	m_Jacket.SetTexture(jacket.c_str());

	//曲名がなかったら「???」
	std::string title = "assets/music_name/" + name + ".png";
	if (!fs::exists(title)) title = "assets/music_name/_unknown.png";
	m_Name.SetTexture(title.c_str());
}

void SongIntro::DrawRect(float cx, float cy, float w, float h, const DirectX::SimpleMath::Vector4& color)
{
	if (color.w <= 0.001f || w <= 0.0f || h <= 0.0f) return;
	Matrix world = Matrix::CreateScale(w, h, 1.0f) * Matrix::CreateTranslation(cx, cy, 0.0f);
	Renderer::SetWorldMatrix(&world);
	Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);
	m_Shader.SetGPU();
	m_RectVB.SetGPU();
	m_RectIB.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Vector4 c = color;
	m_Shader.UpdatePSConstantBuffer(&c, sizeof(c));
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}
