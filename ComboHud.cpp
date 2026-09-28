#include "ComboHud.h"
#include "Renderer.h"
#include "Camera.h"

using namespace DirectX::SimpleMath;

void ComboHud::Init()
{
	// ── コンボ数字(4桁分)のセットアップ ──
	for (int i = 0; i < 4; i++)
	{
		m_ComboDigits[i].Init();
		m_ComboDigits[i].SetTexture("assets/texture/number_3.png");
		m_ComboDigits[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示は "0"
		m_ComboDigits[i].SetScale(m_NuberSplite_Size, m_NuberSplite_Size, 1.0f);
	}

	auto initDigitRow = [this](Texture2D* row)
		{
			for (int i = 0; i < 4; i++)
			{
				row[i].Init();
				row[i].SetTexture("assets/texture/allnumber_2.png");
				row[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f);
				row[i].SetScale(m_NuberSplite_Size * 0.6f, m_NuberSplite_Size * 0.6f, 1.0f); // コンボより小さめ
			}
		};
	initDigitRow(m_PerfectDigits);
	initDigitRow(m_GreatDigits);
	initDigitRow(m_GoodDigits);
	initDigitRow(m_MissDigits);

	Renderer::SetBlendState(BS_ADDITIVE);
	m_BreakdownLabelPerfect.Init();
	m_BreakdownLabelPerfect.SetTexture("assets/texture/PER.png");
	m_BreakdownLabelPerfect.SetScale(70.0f, 18.0f, 1.0f);

	m_BreakdownLabelGreat.Init();
	m_BreakdownLabelGreat.SetTexture("assets/texture/GRE.png");
	m_BreakdownLabelGreat.SetScale(70.0f, 18.0f, 1.0f);

	m_BreakdownLabelGood.Init();
	m_BreakdownLabelGood.SetTexture("assets/texture/GOO.png");
	m_BreakdownLabelGood.SetScale(70.0f, 18.0f, 1.0f);

	m_BreakdownLabelMiss.Init();
	m_BreakdownLabelMiss.SetTexture("assets/texture/MIS.png");
	m_BreakdownLabelMiss.SetScale(70.0f, 18.0f, 1.0f);

	m_JudgeRankPerfect.Init();
	m_JudgeRankPerfect.SetTexture("assets/texture/PER.png");
	m_JudgeRankPerfect.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGreat.Init();
	m_JudgeRankGreat.SetTexture("assets/texture/GRE.png");
	m_JudgeRankGreat.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGood.Init();
	m_JudgeRankGood.SetTexture("assets/texture/GOO.png");
	m_JudgeRankGood.SetScale(200.0f, 50.0f, 1.0f);

	m_BarrierQuad.Init();

	m_FullComboTex.Init();
	m_FullComboTex.SetTexture("assets/texture/FullCpmbo.png");
	m_FullComboTex.SetRotation(0.0f, 0.0f, 0.0f);
	m_FullComboTex.SetScale(360.0f, 260.0f, 0.0f);
	Renderer::SetBlendState(BS_NONE);
}

void ComboHud::TriggerRankPop(int laneIndex, int rank)
{
	if (laneIndex < 0 || laneIndex >= 6) return;
	m_JudgeRankEffects[laneIndex].active = true;
	m_JudgeRankEffects[laneIndex].rank = rank;
	m_JudgeRankEffects[laneIndex].timer = kJudgeRankPopDuration;
}

void ComboHud::TriggerBarrier(int laneIndex)
{
	if (laneIndex < 0 || laneIndex >= 6) return;
	m_BarrierEffects[laneIndex].active = true;
	m_BarrierEffects[laneIndex].timer = 0.0f;
}

void ComboHud::TriggerComboPop()
{
	m_ComboPopTimer = 0.15f; // コンボ数字が一瞬拡大表示される演出用タイマー
}

void ComboHud::TriggerFullCombo()
{
	m_FullComboActive = true;
}

void ComboHud::Update(float deltaTime)
{
	// コンボ数字のポップアニメーションのタイマー減算
	if (m_ComboPopTimer > 0.0f)
	{
		m_ComboPopTimer -= deltaTime;
		if (m_ComboPopTimer < 0.0f) m_ComboPopTimer = 0.0f;
	}

	for (int i = 0; i < 6; i++)
	{
		if (!m_JudgeRankEffects[i].active) continue;
		m_JudgeRankEffects[i].timer -= deltaTime;
		if (m_JudgeRankEffects[i].timer <= 0.0f)
		{
			m_JudgeRankEffects[i].timer = 0.0f;
			m_JudgeRankEffects[i].active = false;
		}
	}
}

void ComboHud::Draw(Camera* cam, const Matrix& view,
	int combo, int perfect, int great, int good, int miss,
	const Vector3 judgeCircleScreenPos[6],
	const Vector3 judgeCirclePos3D[6],
	const Vector3 laneColors[6])
{
	//判定ランク演出
	{
		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 6; i++)
		{
			if (!m_JudgeRankEffects[i].active) continue;

			Texture2D* target = nullptr;
			switch (m_JudgeRankEffects[i].rank)
			{
			case 0: target = &m_JudgeRankPerfect; break;
			case 1: target = &m_JudgeRankGreat;   break;
			case 2: target = &m_JudgeRankGood;    break;
			}
			if (!target) continue;

			Vector3 screenPos = judgeCircleScreenPos[i];
			screenPos.y += 150.0f;

			target->SetPosition(screenPos);
			target->Draw(cam);
		}
		Renderer::SetBlendState(BS_NONE);

		if (m_FullComboActive)
		{
			Renderer::SetBlendState(BS_ALPHABLEND);
			m_FullComboTex.SetPosition(0.0f, 0.0f, 0.0f);
			m_FullComboTex.Draw(cam);
			Renderer::SetBlendState(BS_NONE);
		}
	}

	// ── コンボ数表示（3桁、ヒット直後は少し拡大する演出）──
	{
		int comboClamped = combo;
		if (comboClamped > 999) comboClamped = 999;

		int digits[3] = { comboClamped / 100,(comboClamped / 10) % 10, comboClamped % 10 };
		float popScale = 1.0f + (m_ComboPopTimer / 0.15f) * 0.4f; // 最大1.4倍まで膨らむ

		float digitW = 80.0f;
		float startX = -65.0f;

		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 3; i++)
		{
			m_ComboDigits[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
			m_ComboDigits[i].SetScale(m_NuberSplite_Size * popScale, m_NuberSplite_Size * popScale, 1.0f);
			m_ComboDigits[i].SetPosition(startX + digitW * i, 250.0f, 0.0f);
			m_ComboDigits[i].Draw(cam);
		}
	}

	// ── 判定内訳の表示（PERFECT/GREAT/GOOD/MISS）──
	{
		auto drawCountRow = [&](Texture2D* row, int value, float y)
			{
				if (value > 9999) value = 9999;
				int digits[4] = { value / 1000, value / 100, (value / 10) % 10, value % 10 };
				float digitW = 42.0f;    // 数字同士の間隔（左上コーナーに収まるよう詰める）
				float startX = -545.0f;  // ラベルの右側から数字を並べる

				for (int i = 0; i < 4; i++)
				{
					row[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
					row[i].SetPosition(startX + digitW * i, y, 0.0f);
					row[i].Draw(cam);
				}
			};

		constexpr float kLabelX = -590.0f;		// ラベルのX座標（画面左端に寄せる）
		constexpr float kTopY = 320.0f;		// 一番上の行のY座標（画面上端に寄せる）
		constexpr float kRowSpacing = 40.0f;	// 行間（詰めてまとめる）

		Renderer::SetBlendState(BS_ALPHABLEND);

		m_BreakdownLabelPerfect.SetPosition(kLabelX, kTopY, 0.0f);
		m_BreakdownLabelPerfect.Draw(cam);
		drawCountRow(m_PerfectDigits, perfect, kTopY);

		m_BreakdownLabelGreat.SetPosition(kLabelX, kTopY - kRowSpacing, 0.0f);
		m_BreakdownLabelGreat.Draw(cam);
		drawCountRow(m_GreatDigits, great, kTopY - kRowSpacing);

		m_BreakdownLabelGood.SetPosition(kLabelX, kTopY - kRowSpacing * 2, 0.0f);
		m_BreakdownLabelGood.Draw(cam);
		drawCountRow(m_GoodDigits, good, kTopY - kRowSpacing * 2);

		m_BreakdownLabelMiss.SetPosition(kLabelX, kTopY - kRowSpacing * 3, 0.0f);
		m_BreakdownLabelMiss.Draw(cam);
		drawCountRow(m_MissDigits, miss, kTopY - kRowSpacing * 3);

		Renderer::SetBlendState(BS_NONE);
	}

	Renderer::SetBlendState(BS_ADDITIVE);
	for (int i = 0; i < 6; i++)
	{
		if (!m_BarrierEffects[i].active) continue;
		float progress = m_BarrierEffects[i].timer / kBarrierDuration;
		m_BarrierQuad.Draw(judgeCirclePos3D[i], 300.0f, progress, laneColors[i], view);
	}
	Renderer::SetBlendState(BS_NONE);
}