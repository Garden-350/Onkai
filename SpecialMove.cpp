#include "SpecialMove.h"
#include "ChartObject.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#undef min
#undef max

void SpecialMove::Reset(int totalNote)
{
	m_TotalNote = totalNote;
	m_State = State::Gauge;
	m_Gauge = 0.0f;
	m_ChargeStart = 0.0f;
	m_ChargeEnd = 0.0f;
	m_ChargeWeight = 0.0f;
	m_ChargeNoteCount = 0;
	m_FireTimer = 0.0f;
}

bool SpecialMove::TryActivate(float musicTime, float bpm, int beatsPerBar, std::vector<ChartNote>& notes)
{
	if (!IsReady()) return false;
	if (musicTime < 0.0f) return false;		// カウントイン中は発動しない

	float beat = (bpm > 0.0f) ? 60.0f / bpm : 0.5f;
	float bar = beat * (float)std::max(1, beatsPerBar) * kChargeBars;
	m_ChargeStart = std::ceil(musicTime / beat) * beat;	// 次の拍（曲の0秒が1拍目の前提）
	m_ChargeEnd = m_ChargeStart + bar;

	m_ChargeWeight = 0.0f;
	m_ChargeNoteCount = 0;
	for (auto& n : notes)
	{
		if (n.judged || n.IsLong()) continue;		// v1 はタップとフリックだけ
		if (n.time >= m_ChargeStart && n.time < m_ChargeEnd)
		{
			n.chargeShot = true;
			m_ChargeNoteCount++;
		}
	}

	m_Gauge = 0.0f;
	SetState(State::Waiting, musicTime);
	return true;
}

void SpecialMove::Update(float deltaTime, float musicTime)
{
	switch (m_State)
	{
	case State::Gauge:
		break;

	case State::Waiting:
		if (musicTime >= m_ChargeStart)
		{
			SetState(State::Charging, musicTime);
			if (m_OnChargeStart) m_OnChargeStart();
		}
		break;

	case State::Charging:
		if (musicTime >= m_ChargeEnd)
		{
			m_FireTimer = 0.0f;
			SetState(State::Firing, musicTime);
			if (m_OnFire) m_OnFire(GetChargeRatio());
		}
		break;

	case State::Firing:
		m_FireTimer += deltaTime;
		if (m_FireTimer >= kFireDuration)
		{
			SetState(State::Gauge, musicTime);
			if (m_OnFinish) m_OnFinish();
		}
		break;
	}
}

void SpecialMove::AddHit(int rank, bool isChargeNote)
{
	if (isChargeNote)
	{
		// チャージノーツは少し早く叩くと Waiting 中に判定されることがあるので、どちらでも数える
		if (m_State == State::Waiting || m_State == State::Charging)
			m_ChargeWeight += RankWeight(rank);
		return;
	}
	// 溜め中だけゲージが増える（発動中は増えない）
	if (m_State == State::Gauge && m_TotalNote > 0)
		m_Gauge = std::min(1.0f, m_Gauge + 1.0f / (kFillRatio * (float)m_TotalNote));
}

float SpecialMove::GetChargeRatio() const
{
	if (m_ChargeNoteCount <= 0) return 0.0f;
	return std::clamp(m_ChargeWeight / (float)m_ChargeNoteCount, 0.0f, 1.0f);
}

float SpecialMove::GetChargeProgress(float musicTime) const
{
	float len = m_ChargeEnd - m_ChargeStart;
	if (len <= 0.0001f) return 0.0f;
	return std::clamp((musicTime - m_ChargeStart) / len, 0.0f, 1.0f);
}

float SpecialMove::RankWeight(int rank)
{
	if (rank == 0) return 1.0f;
	if (rank == 1) return 0.6f;
	if (rank == 2) return 0.3f;
	return 0.0f;
}

void SpecialMove::SetState(State s, float musicTime)
{
	m_State = s;
	const char* names[] = { "Gauge", "Waiting", "Charging", "Firing" };
	char buf[200];
	sprintf_s(buf, "[Special] %s  t=%.2f  start=%.2f end=%.2f notes=%d ratio=%.2f\n",
		names[(int)s], musicTime, m_ChargeStart, m_ChargeEnd, m_ChargeNoteCount, GetChargeRatio());
	OutputDebugStringA(buf);
}
