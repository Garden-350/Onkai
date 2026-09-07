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
		m_ComboDigits[i].SetTexture("assets/texture/allnumber_new.png");
		m_ComboDigits[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示は "0"
		m_ComboDigits[i].SetScale(m_NuberSplite_Size, m_NuberSplite_Size, 1.0f);
	}

	auto initDigitRow = [this](Texture2D* row)
		{
			for (int i = 0; i < 4; i++)
			{
				row[i].Init();
				row[i].SetTexture("assets/texture/allnumber_new.png");
				row[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f);
				row[i].SetScale(m_NuberSplite_Size * 0.6f, m_NuberSplite_Size * 0.6f, 1.0f); // コンボより小さめ
			}
		};
	initDigitRow(m_PerfectDigits);
	initDigitRow(m_GreatDigits);
	initDigitRow(m_GoodDigits);
	initDigitRow(m_MissDigits);

	m_JudgeRankPerfect.Init();
	m_JudgeRankPerfect.SetTexture("assets/texture/PERFECT.png");
	m_JudgeRankPerfect.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGreat.Init();
	m_JudgeRankGreat.SetTexture("assets/texture/GREAT.png");
	m_JudgeRankGreat.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGood.Init();
	m_JudgeRankGood.SetTexture("assets/texture/GOOD.png");
	m_JudgeRankGood.SetScale(200.0f, 50.0f, 1.0f);

	m_BarrierQuad.Init();
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

void ComboHud::Update(float deltaTime)
{
	// コンボ数字のポップアニメーションのタイマー減算
	if (m_ComboPopTimer > 0.0f)
	{
		m_ComboPopTimer -= deltaTime;
		if (m_ComboPopTimer < 0.0f) m_ComboPopTimer = 0.0f;
	}

	//ボタン入力時に出る障壁
	//for (int i = 0; i < 6; i++)
	//{
	//	if (!m_BarrierEffects[i].active) continue;
	//	m_BarrierEffects[i].timer += deltaTime;
	//	if (m_BarrierEffects[i].timer >= kBarrierDuration)
	//		m_BarrierEffects[i].active = false;
	//}

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
	}

	// ── コンボ数表示（3桁、ヒット直後は少し拡大する演出）──
	{
		int comboClamped = combo;
		if (comboClamped > 999) comboClamped = 999;

		int digits[3] = { comboClamped / 100,(comboClamped / 10) % 10, comboClamped % 10 };
		float popScale = 1.0f + (m_ComboPopTimer / 0.15f) * 0.4f; // 最大1.4倍まで膨らむ

		float digitW = 40.0f;
		float startX = 200.0f;

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
				float digitW = 26.0f;   // コンボより詰めて表示
				float startX = 40.0f;

				for (int i = 0; i < 4; i++)
				{
					row[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
					row[i].SetPosition(startX + digitW * i, y - 100.0f, 0.0f);
					row[i].Draw(cam);
				}
			};
		float topY = 400.0f;	//一番上のY座標
		float rowSpacing = 80.0f;	//行間

		Renderer::SetBlendState(BS_ALPHABLEND);
		drawCountRow(m_PerfectDigits, perfect, topY);
		drawCountRow(m_GreatDigits, great, topY - rowSpacing);
		drawCountRow(m_GoodDigits, good, topY - rowSpacing * 2);
		drawCountRow(m_MissDigits, miss, topY - rowSpacing * 3);
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