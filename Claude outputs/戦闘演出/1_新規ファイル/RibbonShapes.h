#pragma once
#include <vector>
#include <cmath>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

// ============================================================
// RibbonShapes : LongBeamRibbon で描く「円」「円弧」「多角形」の点を作る道具
//   形は、いつもカメラの方を向いた面の上に作る（どこから見ても丸く見える）
//   角度は「真上が0、時計回りがプラス」（ラジアン）
//   使い方：点の入れ物を clear → Arc や Polygon で点を足す → LongBeamRibbon::Draw に渡す
// ============================================================
namespace RibbonShapes
{
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Color = DirectX::SimpleMath::Color;
	using Points = std::vector<LongBeamRibbon::Point>;

	constexpr float kPi = 3.14159265f;

	// center から見て、カメラの方を向いた面の「右」と「上」を求める
	inline void CameraBasis(const Vector3& center, const Vector3& camPos, Vector3& right, Vector3& up)
	{
		Vector3 n = camPos - center;	// カメラへ向かう向き
		if (n.LengthSquared() < 0.0001f) n = Vector3(0.0f, 0.0f, -1.0f);
		n.Normalize();

		right = n.Cross(Vector3::Up);	// 画面の右
		if (right.LengthSquared() < 0.0001f) right = Vector3(1.0f, 0.0f, 0.0f);
		right.Normalize();

		up = right.Cross(n);			// 画面の上
		up.Normalize();
	}

	// 円弧の点を out に足す
	//   startRad : 始まりの角度（真上が0）、sweepRad : どれだけ回るか（2π で1周）
	inline void Arc(Points& out, const Vector3& center, const Vector3& right, const Vector3& up,
		float radius, float startRad, float sweepRad, int segments, const Color& color)
	{
		if (segments < 1) segments = 1;
		for (int i = 0; i <= segments; i++)
		{
			float a = startRad + sweepRad * (i / (float)segments);
			LongBeamRibbon::Point p;
			p.pos = center + (right * std::sin(a) + up * std::cos(a)) * radius;
			p.color = color;
			out.push_back(p);
		}
	}

	// 正多角形（閉じた形）の点を out に足す。sides = 6 で六角形
	inline void Polygon(Points& out, const Vector3& center, const Vector3& right, const Vector3& up,
		float radius, int sides, float rotRad, const Color& color)
	{
		Arc(out, center, right, up, radius, rotRad, 2.0f * kPi, sides, color);
	}

	// Vector3 の色と濃さから Color を作る
	inline Color MakeColor(const Vector3& rgb, float alpha)
	{
		return Color(rgb.x, rgb.y, rgb.z, alpha);
	}
}
