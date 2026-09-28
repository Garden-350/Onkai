#include "JudgeSystem.h"
#include "ChartObject.h"
#include "Sound.h"
#include "Input.h"
#include "ResultData.h"
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
		sound.Play(SOUND_CLAMPU);
	}
	else if(rank == 1)
	{
		m_Great++;
		sound.Play(SOUND_CLAMPU);
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
		int requiredSide = n.IsFlickRight() ? -1 : 1;	//右フリックなら1、左フリックなら-1

		if (playerSide != requiredSide) continue;		//まだ正しい位置にいない
		if (diff > 0.0f) continue;						//先についてても判定しない

		float late = -diff;								//届いてから何秒遅れて正しい位置に来たか
		if (late > kFlickGood) continue;				//遅すぎ
		int rank = (late < kFlickPerfect) ? 0 : (late < kFlickGreat) ? 1 : 2;

		int laneFrom = n.lane;
		int laneTo = (n.flickLaneTo >= 0) ? n.flickLaneTo : n.lane;
		int lo = std::min(laneFrom, laneTo);
		int hi = std::max(laneFrom, laneTo);

		if (rank == 0) m_Perfect++;
		else if (rank == 1) m_Great++;
		else m_Good++;
		for (int L = lo; L <= hi; L++)
		{
			if (L < 0 || L >= 6) continue;
			RegisterHit(L, rank);
		}
		n.judged = true;
	}
}

void JudgeSystem::JudgeLongNoteHold(std::vector<ChartNote>& notes, float musicTime)
{
	constexpr float kEps = 0.0001f;
	for (auto& n : notes)
	{
		if (!n.IsLong()) continue;
		if (n.judged) continue;

		if (n.lane < 0 || n.lane >= 6) continue;
		bool isDown = Input::GetKeyPress(kLaneKeys[n.lane]);

		while (musicTime >= n.nextTickTime - kEps)
		{
			bool isFinalTick = (n.nextTickTime >= n.time - kEps);

			if (isDown)
			{
				m_Perfect++;
				RegisterHit(n.lane, 0);
			}
			else
			{
				m_Miss++;
				RegisterMiss();
			}

			if (isFinalTick)
			{
				n.judged = true;
				break;
			}

			n.nextTickTime += kLongTickInterval;
			if(n.nextTickTime > n.time - kEps)
			{
				n.nextTickTime = n.time;	//最後は必ず終点
			}
		}

		if (!n.judged && musicTime >= n.time - kEps)
		{
			n.judged = true;	//保険　最後までチェックポイントが尽きないまま終了時刻を過ぎた場合
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
			if (note.lane < 0 || note.lane >= 6) continue;
			constexpr float kEps = 0.0001f;
			// オートモードなので全チェックポイントを常に成功扱いで消化する
			while (musicTime >= note.nextTickTime - kEps)
			{
				bool isFinalTick = (note.nextTickTime >= note.time - kEps);

				m_Perfect++;
				RegisterHit(note.lane, 0);
				sound.Play(isFinalTick ? SOUND_LONG_END : SOUND_SUZU);

				if (isFinalTick)
				{
					note.judged = true;
					break;
				}

				note.nextTickTime += kLongTickInterval;
				if (note.nextTickTime > note.time - kEps)
					note.nextTickTime = note.time;
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
			RegisterMiss();	//コンボを切る
		}
	}
}

void JudgeSystem::MarkFlickAutoMiss(std::vector<ChartNote>& notes, float musicTime)
{
	for (auto& note : notes)
	{
		if (note.judged) continue;
		if (!note.IsFlick()) continue;
		if (musicTime - note.time > kFlickGood)
		{
			note.judged = true;
			m_Miss++;
			RegisterMiss();	//コンボを切る
		}
	}
}

void JudgeSystem::ResetJudgeCounts()
{
	m_Perfect = 0;
	m_Great = 0;
	m_Good = 0;
	m_Miss = 0;
	m_Combo = 0;
	m_MaxCombo = 0;
}

void JudgeSystem::RegisterHit(int laneIndex, int rank)
{
	m_Combo++;
	if (m_Combo > m_MaxCombo) m_MaxCombo = m_Combo;
	if (m_OnJudge) m_OnJudge(laneIndex, rank);
}

void JudgeSystem::RegisterMiss()
{
	m_Combo = 0;
}
