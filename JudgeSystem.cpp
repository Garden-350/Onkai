#include "JudgeSystem.h"
#include "ChartObject.h"
#include "Sound.h"
#include "Input.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

namespace
{
		// レーン(0〜5) <-> S/D/F/J/K/Lキーの対応表
	constexpr int kLaneKeys[6] = { VK_S, VK_D, VK_F, VK_J, VK_K, VK_L };
}

void JudgeSystem::JudgeLane(int key, int laneIndex, std::vector<ChartNote>& notes, float musicTime, Sound& sound)
{
	if (!Input::GetKeyTrigger(key)) return;

	int bestIndex = -1;
	float bestDiff = 9999.0f;

	// 対象レーンのTAPノーツの中から、最も時間差が小さいものを探す
	for(int i = 0; i < (int)notes.size(); i++)
	{
		if (notes[i].judged) continue;
		if (notes[i].lane != laneIndex) continue;
		if (!notes[i].IsTap()) continue;
		float diff = std::abs(musicTime - notes[i].time);
		if (diff < bestDiff)
		{
			bestDiff = diff;
			bestIndex = i;
		}
	}
	if (bestIndex == -1)
	{
		if(m_OnEmptyPress) m_OnEmptyPress(laneIndex);
		return;
	}
	if (bestDiff > kGud) return;	//判定圏外ならなにもしない

	//時間差に応じてランクを決定
	int rank = (bestDiff <= kPerfect) ? 0 : (bestDiff <= kGreat) ? 1 : 2;
	if (rank == 0)
	{
		m_Perfect++;
		sound.Play(SOUND_METAL_1);
	}
	else if(rank == 1)
	{
		m_Great++;
		sound.Play(SOUND_METAL_1);
	}
	else if (rank == 2)
	{
		m_Good++;
		sound.Play(SOUND_METAL_1);
	}
	notes[bestIndex].judged = true;
	RegisterHit(laneIndex, rank);
}

void JudgeSystem::JudgeFlickNotes(std::vector<ChartNote>& notes, float musicTime, int playerSide)
{
	for (auto& n : notes)
	{
		if (n.judged) continue;
		if (!n.IsFlick())continue;

		float diff = n.time - musicTime;	//正：まだ来ていない　負：過ぎた
		int requiredSide = n.IsFlickRight() ? 1 : -1;	//右フリックなら1、左フリックなら-1

		if(fabsf(diff) <= kGud && playerSide == requiredSide)
		{
			float absDiff = fabsf(diff);
			int rank = (absDiff < kPerfect) ? 0 : (absDiff < kGreat) ? 1 : 2;
			
			int laneFrom = n.lane;
			int laneTo = (n.flickLaneTo >= 0) ? n.flickLaneTo : n.lane;
			int lo = std::min(laneFrom, laneTo);
			int hi = std::max(laneFrom, laneTo);

			if(rank == 0) m_Perfect++;
			else if(rank == 1) m_Great++;
			else m_Good++;
			for(int L = lo; L <= hi; L++)
			{
				if(L < 0 || L >= 6) continue;
				RegisterHit(L, rank);
			}
			n.judged = true;
			continue;
		}
	}
}

void JudgeSystem::JudgeLongNoteHold(std::vector<ChartNote>& notes, float musicTime)
{
	for (auto& n : notes)
	{
		if (!n.IsLong()) continue;
		if (n.judged) continue;

		if (n.lane < 0 || n.lane >= 6) continue;
		bool isDown = Input::GetKeyPress(kLaneKeys[n.lane]);

		// ── まだ開始していない場合 ──
		if (!n.longStarted && !n.longFailed)
		{
			float diff = n.arrivalTime - musicTime;
			if (isDown && fabsf(diff) <= kGud)
			{
				float absDiff = fabsf(diff);
				int rank = (absDiff < kPerfect) ? 0 : (absDiff < kGreat) ? 1 : 2;
				if (rank == 0) m_Perfect++;
				else if (rank == 1) m_Great++;
				else m_Good++;
				RegisterHit(n.lane, rank);

				n.longStarted = true;
				n.nextTickTime = n.arrivalTime + kLongTickInterval;
			}
			else if (diff < -kGud)
			{
				// 開始を逃した → Failed扱いにするだけ。ノーツはendTimeまで残す
				n.longFailed = true;
				m_Miss++;
			}
		}
		// ── 開始済み・まだ成功継続中 ──
		else if (n.longStarted && !n.longFailed)
		{
			if (isDown)
			{
				while (n.nextTickTime <= musicTime && n.nextTickTime < n.time)
				{
					m_Perfect++;
					n.nextTickTime += kLongTickInterval;
				}
			}
			else
			{
				// 離した瞬間 → Failedにするが、ノーツはendTimeまで残す
				n.longFailed = true;
				m_Miss++;
			}
		}

		// ── 終了時刻に到達したら、ここで初めて judged=true にする ──
		if (musicTime >= n.time)
		{
			if (!n.longFailed) m_Perfect++; // 最後まで成功していればクリア扱い
			n.judged = true;
		}
	}
}

void JudgeSystem::UpdateAutoJudgge(std::vector<ChartNote>& notes, float musicTime, Sound& sound)
{
	for (auto& note : notes)
	{
		if (note.judged) continue;

		if (note.IsTap())
		{
			if (musicTime >= note.time)
			{
				m_Perfect++;
				sound.Play(SOUND_METAL_1);
				RegisterHit(note.lane, 0);
				note.judged = true;
			}
		}
		else if (note.IsFlick())
		{
			if (musicTime >= note.time)
			{
				int laneFrom = note.lane;
				int laneTo = (note.flickLaneTo >= 0) ? note.flickLaneTo : note.lane;
				int lo = std::min(laneFrom, laneTo);
				int hi = std::max(laneFrom, laneTo);

				m_Perfect++;
				for (int L = lo; L <= hi; L++)
				{
					if (L < 0 || L >= 6) continue;
					RegisterHit(L, 0);
				}
				note.judged = true;
			}
		}
		else if (note.IsLong())
		{
			// 先端到達で自動的に押し始めが成立
			if (!note.longStarted && musicTime >= note.arrivalTime)
			{
				m_Perfect++;
				sound.Play(SOUND_SUZU);
				RegisterHit(note.lane, 0);
				note.longStarted = true;
				note.nextTickTime = note.arrivalTime + kLongTickInterval;
			}

			// 押しっぱなし中の継続判定（オートなので常に成功）
			if (note.longStarted)
			{
				while (note.nextTickTime <= musicTime && note.nextTickTime < note.time)
				{
					m_Perfect++;
					sound.Play(SOUND_SUZU);
					note.nextTickTime += kLongTickInterval;
				}
			}

			// 終了時刻に到達したら確定（longFailedには絶対にならない）
			if (musicTime >= note.time)
			{
				if (note.longStarted) m_Perfect++;
				sound.Play(SOUND_LONG_END);
				note.judged = true;
			}
		}
	}
}

void JudgeSystem::MarkTapAutoMiss(std::vector<ChartNote>& notes, float musicTime)
{
	for (auto& note : notes)
	{
		if (note.judged) continue;
		if (!note.IsTap()) continue;
		if (musicTime - note.time > kGud)
		{
			note.judged = true;
			m_Miss++;
		}
	}
}

void JudgeSystem::ResetJudgeCounts()
{
	m_Perfect = 0;
	m_Great = 0;
	m_Good = 0;
	m_Miss = 0;
}

void JudgeSystem::RegisterHit(int laneIndex, int rank)
{
	m_Combo++;
	if (m_Combo > m_MaxCombo) m_MaxCombo = m_Combo;
	if (m_OnJudge) m_OnJudge(laneIndex, rank);
}
