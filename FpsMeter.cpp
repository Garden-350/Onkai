#include "FpsMeter.h"
#include "Application.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#undef min
#undef max

void FpsMeter::Init()
{
	//スコアと同じ数字の画像を使う。頭の 0 は出さない
	m_FpsDigits.Init();
	m_FpsDigits.SetUp("assets/texture/allnumber_new.png", 3);
	m_FpsDigits.SetDigitSize(36.0f, 36.0f);
	m_FpsDigits.SetLeadingZeroAlpha(0.0f);

	m_MsDigits.Init();
	m_MsDigits.SetUp("assets/texture/allnumber_new.png", 2);
	m_MsDigits.SetDigitSize(24.0f, 24.0f);
	m_MsDigits.SetLeadingZeroAlpha(0.0f);

	//元のタイトルを覚えておく（表示を消した時に戻す）
	wchar_t title[256] = {};
	GetWindowTextW(Application::GetWindow(), title, 256);
	m_BaseTitle = title;
}

void FpsMeter::BeginFrame()
{
	auto now = Clock::now();
	if (!m_Started) { m_WindowStart = now; m_Started = true; }
	m_FrameStart = now;
}

void FpsMeter::EndFrame()
{
	auto now = Clock::now();
	float work = std::chrono::duration<float, std::milli>(now - m_FrameStart).count();	//Update + Draw にかかった時間(ms)
	m_Frames++;
	m_WorkSum += work;
	m_WorkMax = std::max(m_WorkMax, work);

	float elapsed = std::chrono::duration<float>(now - m_WindowStart).count();
	if (elapsed < kInterval) return;	//0.5 秒たつまで貯める

	int fps = (int)std::lround(m_Frames / elapsed);	//この 0.5 秒に何フレーム回ったか → 1 秒あたりに直す
	float avg = m_WorkSum / m_Frames;
	m_FpsDigits.SetValue(fps, true);
	m_MsDigits.SetValue((int)std::ceil(m_WorkMax), true);	//切り上げ（6.2ms なら 7）

	if (m_Visible)
	{
		wchar_t buf[256];
		swprintf_s(buf, L"%s   FPS %d | work avg %.1f ms / max %.1f ms", m_BaseTitle.c_str(), fps, avg, m_WorkMax);
		SetWindowTextW(Application::GetWindow(), buf);
	}

	//次の 0.5 秒の集計へ
	m_Frames = 0;
	m_WorkSum = 0.0f;
	m_WorkMax = 0.0f;
	m_WindowStart = now;
}

void FpsMeter::Toggle()
{
	m_Visible = !m_Visible;
	if (!m_Visible) SetWindowTextW(Application::GetWindow(), m_BaseTitle.c_str());	//タイトルを元に戻す
}

void FpsMeter::Draw(Camera* cam)
{
	if (!m_Visible) return;

	//2D は画面の真ん中が 0,0。左上の角から 24px 内側に置く
	float W = (float)Application::GetWidth(), H = (float)Application::GetHeight();
	float left = -W * 0.5f + 24.0f, top = H * 0.5f - 24.0f;

	Renderer::SetDepthOverlayEnable(true);	//何があっても一番手前
	m_FpsDigits.SetPosition(left + 45.0f, top - 18.0f);	//3 桁の列の真ん中
	m_FpsDigits.Draw(cam);
	m_MsDigits.SetPosition(left + 21.0f, top - 56.0f);	//2 桁の列の真ん中
	m_MsDigits.Draw(cam);
	Renderer::SetDepthOverlayEnable(false);
	Renderer::SetBlendState(BS_NONE);
}