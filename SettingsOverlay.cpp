#include "SettingsOverlay.h"
#include "PlayerSettings.h"
#include "Game.h"
#include "Input.h"
#include "Renderer.h"
#include "sound.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	// ---- 置き場所（画面の真ん中が 0,0。右が＋x、上が＋y） ----
	constexpr float kZ = 1.0f;							// 一番手前（深度は見ないので、値は描く順だけの意味）
	constexpr float kPanelW = 820.0f, kPanelH = 500.0f;
	constexpr float kTitleY = 185.0f;
	constexpr float kRowY0 = 95.0f, kRowGap = 95.0f;	// 1 行目の高さと、行の間
	constexpr float kLabelX = -170.0f;					// 項目の名前の真ん中
	constexpr float kValueX = 200.0f;					// 値の真ん中
	constexpr float kDigit = 56.0f;						// 数字 1 桁の大きさ
	constexpr float kHintY = -195.0f;

	float RowY(int i) { return kRowY0 - kRowGap * i; }

	// 絵を読む（Init → SetTexture → 大きさ）
	void Setup(Texture2D& t, const char* path, float w, float h)
	{
		t.Init();
		t.SetTexture(path);
		t.SetScale(w, h, 0.0f);
	}
	void DrawAt(Texture2D& t, Camera* cam, float x, float y, float alpha = 1.0f)
	{
		t.SetPosition(x, y, kZ);
		t.SetAlpha(alpha);
		t.Draw(cam);
	}

	bool AnyLaneKeyTrigger()
	{
		const int keys[] = { VK_S, VK_D, VK_F, VK_J, VK_K, VK_L };
		for (int k : keys) if (Input::GetKeyTrigger(k)) return true;
		return false;
	}
}

void SettingsOverlay::Init()
{
	Setup(m_Dim, "assets/settings/dim.png", 1280.0f, 720.0f);	// 後ろを暗くする

	Setup(m_Panel, "assets/settings/panel.png", kPanelW, kPanelH);
	Setup(m_Title, "assets/settings/title.png", 260.0f, 64.0f);
	Setup(m_Hint, "assets/settings/hint_menu.png", 780.0f, 40.0f);
	Setup(m_Frame, "assets/texture/option_frame.png", 760.0f, 88.0f);
	Setup(m_Labels[kBrightness], "assets/settings/label_brightness.png", 300.0f, 60.0f);
	Setup(m_Labels[kSpeed], "assets/settings/label_speed.png", 300.0f, 60.0f);
	Setup(m_Labels[kTiming], "assets/settings/label_timing.png", 300.0f, 60.0f);
	Setup(m_Percent, "assets/settings/unit_percent.png", 40.0f, 50.0f);
	Setup(m_Dot, "assets/settings/dot.png", 16.0f, 16.0f);
	Setup(m_Ms, "assets/settings/unit_ms.png", 60.0f, 44.0f);
	Setup(m_Plus, "assets/settings/sign_plus.png", 40.0f, 40.0f);
	Setup(m_Minus, "assets/settings/sign_minus.png", 40.0f, 40.0f);

	Setup(m_CalibTitle, "assets/settings/calib_title.png", 360.0f, 64.0f);
	Setup(m_CalibHint, "assets/settings/calib_hint.png", 780.0f, 40.0f);
	Setup(m_CalibDoneHint, "assets/settings/calib_done.png", 780.0f, 40.0f);
	Setup(m_CalibFew, "assets/settings/calib_few.png", 780.0f, 40.0f);
	Setup(m_LabelCount, "assets/settings/label_count.png", 220.0f, 54.0f);
	Setup(m_LabelResult, "assets/settings/label_result.png", 220.0f, 54.0f);
	Setup(m_TapFlash, "assets/settings/tap_flash.png", 120.0f, 120.0f);

	m_Num3.Init();
	m_Num3.SetUp("assets/texture/allnumber_new.png", 3);
	m_Num3.SetDigitSize(kDigit, kDigit);
	m_Num3.SetLeadingZeroAlpha(0.0f);		// 頭の0は出さない
	m_Num2.Init();
	m_Num2.SetUp("assets/texture/allnumber_new.png", 2);
	m_Num2.SetDigitSize(kDigit, kDigit);
	m_Num2.SetLeadingZeroAlpha(0.0f);
}

void SettingsOverlay::Open()
{
	m_Mode = Mode::Menu;
	m_Cursor = 0;
	PlayerSettings::Get().highSpeed = Game::GetInstance()->GetHighSpeed();	// 今のハイスピードから始める
}

void SettingsOverlay::Close()
{
	m_Mode = Mode::Closed;
	ApplyToGame();
	PlayerSettings::Save();
}

void SettingsOverlay::ApplyToGame()
{
	Game::SetHightSpeed(PlayerSettings::Get().highSpeed);
	// 背景の明るさ・タイミングは、ステージが PlayerSettings::Get() から読む
}

void SettingsOverlay::ChangeValue(int item, int dir, bool big)
{
	PlayerSettingsData& s = PlayerSettings::Get();
	switch (item)
	{
	case kBrightness:
		s.brightness = std::clamp(s.brightness + dir * PlayerSettings::kBrightnessStep * (big ? 4.0f : 1.0f),
			PlayerSettings::kBrightnessMin, PlayerSettings::kBrightnessMax);
		break;
	case kSpeed:
		s.highSpeed = std::clamp(s.highSpeed + dir * PlayerSettings::kSpeedStep * (big ? 5.0f : 1.0f),
			PlayerSettings::kSpeedMin, PlayerSettings::kSpeedMax);
		s.highSpeed = std::round(s.highSpeed * 10.0f) / 10.0f;		// 0.1 刻みにそろえる（誤差がたまらないように）
		break;
	case kTiming:
		s.offsetMs = std::clamp(s.offsetMs + dir * (big ? PlayerSettings::kOffsetBigStep : PlayerSettings::kOffsetStep),
			PlayerSettings::kOffsetMin, PlayerSettings::kOffsetMax);
		break;
	}
	ApplyToGame();
}

// ============================================================
// 入力（曲選びから毎フレーム）
// ============================================================
void SettingsOverlay::Tick(float dt, Sound& sound)
{
	m_Anim += dt;
	m_CalibFlash = std::max(0.0, m_CalibFlash - dt);

	if (m_Mode == Mode::Menu)
	{
		if (Input::GetKeyTrigger(VK_SPACE)) { Close(); return; }
		if (Input::GetKeyTrigger(VK_UP))   m_Cursor = (m_Cursor + kItemCount - 1) % kItemCount;
		if (Input::GetKeyTrigger(VK_DOWN)) m_Cursor = (m_Cursor + 1) % kItemCount;
		bool big = Input::GetKeyPress(VK_SHIFT);
		if (Input::GetKeyTrigger(VK_RIGHT)) ChangeValue(m_Cursor, +1, big);
		if (Input::GetKeyTrigger(VK_LEFT))  ChangeValue(m_Cursor, -1, big);
		if (Input::GetKeyTrigger(VK_TAB))   StartCalib(sound);
	}
	else if (m_Mode == Mode::Calib)
	{
		TickCalib(sound);
	}
}

// ============================================================
// タイミングを測る：メトロノームに合わせて押した時刻と、拍の時刻の差を平均する
//   時刻は「鳴らせと言った時」からの時計で数える（ステージと同じ数え方なので、結果をそのまま判定に使える）
// ============================================================
void SettingsOverlay::StartCalib(Sound& sound)
{
	m_Mode = Mode::Calib;
	m_CalibTaps.clear();
	m_CalibDone = false;
	sound.StopDynamic();
	if (FAILED(sound.LoadDynamic("assets/se/calib_metronome.wav", false)))
	{
		m_CalibRunning = false;
		m_CalibDone = true;			// 鳴らせない：結果なし（回数 0）で終わりにする
		return;
	}
	sound.SetVolumeDynamic(1.0f);
	sound.SetPlayPositionSecondsDynamic(0.0f);
	sound.PlayDynamic();
	m_CalibStart = std::chrono::steady_clock::now();
	m_CalibRunning = true;
}

void SettingsOverlay::StopCalib(Sound& sound)
{
	sound.StopDynamic();
	m_CalibRunning = false;
}

void SettingsOverlay::TickCalib(Sound& sound)
{
	if (Input::GetKeyTrigger(VK_TAB)) { StopCalib(sound); m_Mode = Mode::Menu; return; }	// やめて戻る

	const double spb = 60.0 / kCalibBpm;
	if (m_CalibRunning)
	{
		double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_CalibStart).count();
		if (AnyLaneKeyTrigger())
		{
			m_CalibFlash = 0.1;
			int k = (int)std::lround((t - kCalibLead) / spb);					// 一番近い拍
			double devMs = (t - (kCalibLead + k * spb)) * 1000.0;				// その拍とのずれ
			bool counted = (k >= kCalibPractice && k < kCalibPractice + kCalibCount && std::fabs(devMs) < spb * 350.0);
			if (counted) m_CalibTaps.push_back(devMs);
		}
		// 最後の拍が過ぎたら終わり
		if (t > kCalibLead + spb * (kCalibPractice + kCalibCount) + 0.6)
		{
			StopCalib(sound);
			m_CalibDone = true;
			double sum = 0.0;
			for (double v : m_CalibTaps) sum += v;
			m_CalibResult = m_CalibTaps.empty() ? 0.0 : sum / m_CalibTaps.size();
		}
	}
	else if (m_CalibDone)
	{
		if (Input::GetKeyTrigger(VK_RETURN) && (int)m_CalibTaps.size() >= kCalibMinTaps)
		{
			PlayerSettings::Get().offsetMs = std::clamp((float)std::round(m_CalibResult),
				PlayerSettings::kOffsetMin, PlayerSettings::kOffsetMax);
			m_Mode = Mode::Menu;
			m_Cursor = kTiming;
		}
	}
}

// ============================================================
// 描く
// ============================================================
void SettingsOverlay::Draw(Camera* cam)
{
	if (m_Mode == Mode::Closed) return;

	Renderer::SetDepthOverlayEnable(true);			// ここから先は、何があっても一番手前に描く
	DrawAt(m_Dim, cam, 0.0f, 0.0f, 0.55f);			// 後ろを暗くする
	DrawAt(m_Panel, cam, 0.0f, 0.0f, 0.92f);
	if (m_Mode == Mode::Menu) DrawMenu(cam);
	else DrawCalib(cam);
	Renderer::SetDepthOverlayEnable(false);
}

void SettingsOverlay::DrawMenu(Camera* cam)
{
	const PlayerSettingsData& s = PlayerSettings::Get();
	DrawAt(m_Title, cam, 0.0f, kTitleY);
	DrawAt(m_Frame, cam, 0.0f, RowY(m_Cursor), 0.6f + 0.4f * std::sin(m_Anim * 5.0f));	// 選んでいる行の枠（点滅）

	for (int i = 0; i < kItemCount; i++)
	{
		float a = (i == m_Cursor) ? 1.0f : 0.5f;
		float y = RowY(i);
		DrawAt(m_Labels[i], cam, kLabelX, y, a);

		switch (i)
		{
		case kBrightness:	// 「100 %」
			m_Num3.SetValue((int)std::lround(s.brightness * 100.0f), true);
			m_Num3.SetAlpha(a);
			m_Num3.SetPosition(kValueX, y, kZ);
			m_Num3.Draw(cam);
			DrawAt(m_Percent, cam, kValueX + kDigit * 1.6f, y, a);
			break;
		case kSpeed:		// 「1.5」（2 桁の間に点）
			m_Num2.SetSpacing(kDigit * 1.05f);
			m_Num2.SetLeadingZeroAlpha(1.0f);
			m_Num2.SetValue((int)std::lround(s.highSpeed * 10.0f), true);
			m_Num2.SetAlpha(a);
			m_Num2.SetPosition(kValueX, y, kZ);
			m_Num2.Draw(cam);
			DrawAt(m_Dot, cam, kValueX, y - kDigit * 0.38f, a);
			break;
		case kTiming:		// 「+12 ms」
			DrawSigned(cam, kValueX, y, (int)std::lround(s.offsetMs), kDigit);
			break;
		}
	}
	DrawAt(m_Hint, cam, 0.0f, kHintY);
}

void SettingsOverlay::DrawSigned(Camera* cam, float x, float y, int value, float digitSize)
{
	int v = std::abs(value);
	DrawAt(value < 0 ? m_Minus : m_Plus, cam, x - digitSize * 1.9f, y, (value == 0) ? 0.0f : 1.0f);
	m_Num3.SetLeadingZeroAlpha(0.0f);
	m_Num3.SetValue(v, true);
	m_Num3.SetAlpha(1.0f);
	m_Num3.SetPosition(x, y, kZ);
	m_Num3.Draw(cam);
	DrawAt(m_Ms, cam, x + digitSize * 2.0f, y);
}

void SettingsOverlay::DrawCalib(Camera* cam)
{
	DrawAt(m_CalibTitle, cam, 0.0f, kTitleY);

	// 押した回数
	DrawAt(m_LabelCount, cam, kLabelX, RowY(0));
	m_Num2.SetSpacing(kDigit * 0.75f);
	m_Num2.SetLeadingZeroAlpha(0.0f);
	m_Num2.SetValue((int)m_CalibTaps.size(), true);
	m_Num2.SetAlpha(1.0f);
	m_Num2.SetPosition(kValueX, RowY(0), kZ);
	m_Num2.Draw(cam);

	// 押した時に光る
	if (m_CalibRunning) DrawAt(m_TapFlash, cam, 0.0f, RowY(1), (float)(m_CalibFlash / 0.1));

	if (m_CalibRunning)
	{
		DrawAt(m_CalibHint, cam, 0.0f, kHintY);
	}
	else if ((int)m_CalibTaps.size() >= kCalibMinTaps)
	{
		DrawAt(m_LabelResult, cam, kLabelX, RowY(2));
		DrawSigned(cam, kValueX, RowY(2), (int)std::lround(m_CalibResult), kDigit);
		DrawAt(m_CalibDoneHint, cam, 0.0f, kHintY);
	}
	else
	{
		DrawAt(m_CalibFew, cam, 0.0f, kHintY);
	}
}
