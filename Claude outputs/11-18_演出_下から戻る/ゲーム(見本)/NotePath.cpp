#include "Game.h"
#include "NotePath.h"
#include "ChartObject.h" // ChartNoteの定義を得るため
#include "Gimmick.h"	//速さの演出（低速・止める・逆走）
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace NotePath
{
	// ============================================================
	// EvaluateNotePosition : TAP/FLICK用の位置補間。
	//   spawnTime〜timeの範囲でtを[0,1]にクランプしてから、
	//   制御点の数に応じて直線/2次ベジェ/3次ベジェで補間する。
	// ============================================================
	Vector3 EvaluateNotePosition(const ChartNote& note, float now)
	{
		float startTime = note.GetSpawnTime();
		float endTime = note.time;
		float duration = endTime - startTime;
		if (duration < 0.0001f) duration = 0.0001f; // ゼロ除算防止

		float t = (now - startTime) / duration;
		float tMax = 1.0f;
		if (!note.IsFlick())	//フリックは敵の方へ飛ぶので今まで通り
		{
			//速さの演出：「進んだ量」で進み具合を出す。逆走中は 1 を越えて判定円より手前（下）にも出る
			t = Gimmick::DisplayProgress(startTime, endTime, now);	//「下から戻る」もかけた見た目の進み具合
			tMax = 1.0f + Gimmick::kBelowLimit;
		}
		if (t < 0.0f) t = 0.0f;
		if (t > tMax) t = tMax;

		float highSpeed = Game::GetInstance()->GetHighSpeed();
		//const auto& p0 = note.spawnPos;
		const auto& p3 = note.judgePos;
		const auto& p0 = p3 + (note.spawnPos - p3) * highSpeed;	//出現距離を変更

		// 制御点なし → 直線補間
		if (note.controlPoints.empty())
		{
			return Vector3::Lerp(p0, p3, t);
		}
		// 制御点1個 → 2次ベジェ
		if (note.controlPoints.size() == 1)
		{
			const auto& p1 = note.controlPoints[0];
			return Vector3::Lerp(Vector3::Lerp(p0, p1, t), Vector3::Lerp(p1, p3, t), t);
		}
		// 制御点2個 → 3次ベジェ（De Casteljauアルゴリズム）
		const auto& p1 = note.controlPoints[0];
		const auto& p2 = note.controlPoints[1];
		auto a = Vector3::Lerp(p0, p1, t);
		auto b = Vector3::Lerp(p1, p2, t);
		auto c = Vector3::Lerp(p2, p3, t);
		return Vector3::Lerp(Vector3::Lerp(a, b, t), Vector3::Lerp(b, c, t), t);
	}

	// ============================================================
	// EvaluateLongNotePosition : LONGNOTE専用の位置補間。
	//   spawnTime〜arrivalTimeの速度を基準に、tを「クランプせず」直線補間する。
	//   これにより、先端(sentan)がarrivalTimeを過ぎても同じ速度で
	//   判定ラインの奥へ進み続けられる（＝押している間、鎖が伸び続けるように）。
	//   spiralRadiusが設定されている場合は、進行方向を軸にした螺旋オフセットを加える
	//   （フィナーレ演出用。spiralRadius<=0なら従来通りの直線移動のまま）。
	// ============================================================
	Vector3 EvaluateLongNotePosition(const ChartNote& note, float now)
	{
		float startTime = note.GetSpawnTime();
		float arriveTime = note.arrivalTime;
		float duration = arriveTime - startTime;
		if (duration < 0.0001f) duration = 0.0001f;

		float highSpeed = Game::GetInstance()->GetHighSpeed();
		const Vector3& judgePos = note.judgePos;
		Vector3 spawnPos = judgePos + (note.spawnPos - judgePos) * highSpeed;

		float t = Gimmick::DisplayProgress(startTime, arriveTime, now); // クランプしない（速さの演出も考える。演出なしなら今までと同じ）
		Vector3 basePos = Vector3::Lerp(spawnPos, judgePos, t);

		if (note.spiralRadius > 0.0001f)
		{
			Vector3 axis = judgePos - spawnPos;
			if (axis.LengthSquared() > 0.0001f)
			{
				axis.Normalize();

				// 軸に垂直な基準ベクトルを2本作る（軸がYに近い場合はRightを仮ベクトルにする）
				Vector3 arbitrary = (std::fabs(axis.y) < 0.99f) ? Vector3::Up : Vector3::Right;
				Vector3 right = axis.Cross(arbitrary);
				right.Normalize();
				Vector3 up = axis.Cross(right);
				up.Normalize();

				// tが1(到達)を超えても回転を続けさせるためクランプしないまま使う
				float angle = t * note.spiralTurns * DirectX::XM_2PI;
				float radius = note.spiralRadius * std::max(0.0f, 1.0f - t); // 判定時刻でちょうど0に収束
				Vector3 offset = (right * cosf(angle) + up * sinf(angle)) * radius;
				basePos += offset;
			}
		}

		return basePos;
	}
}
