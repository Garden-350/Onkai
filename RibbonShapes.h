#pragma once
#include <vector>
#include <cmath>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

//使い方：点をclear->ArcやPolygonで点を足す->LongBeamRibbon::Drawに渡す
namespace RibbonShapes
{
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Color = DirectX::SimpleMath::Color;
	using Points = std::vector<LongBeamRibbon::Point>;

	constexpr float kPi = 3.14159265f;

	//centerから見てカメラの方を向いた面の「右」と「上」を求める
	inline void CameraBasis(const Vector3& center, const Vector3& camPos, Vector3& right, Vector3& up)
	{
		Vector3 n = camPos - center;	//カメラへ向かう向き
		if (n.LengthSquared() < 0.00001f) n = Vector3(0.0f, 0.0f, -1.0f);
		n.Normalize();

		right = n.Cross(Vector3::Up);	//画面の右
		if (right.LengthSquared() < 0.00001f) right = Vector3(1.0f, 0.0f, 0.0f);
		right.Normalize();

		up = right.Cross(n);	//画面の上
		up.Normalize();
	}

	//円弧の点をoutに足す
	//startRad:始まりの角度（真上：0）sweepRad:どれだけ回るか（2πで一周）
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

	//正多角形の点をoutに足す。sides=6の六角形
	inline void Polygon(Points& out, const Vector3& center, const Vector3& right, const Vector3& up,
		float radius, int sides, float rotRad, const Color& color)
	{
		Arc(out, center, right, up, radius, rotRad, 2.0f * kPi, sides, color);
	}

	//Vector3の色と濃さからColorを作る
	inline Color MakeColor(const Vector3& rgb, float alpha)
	{
		return Color(rgb.x, rgb.y, rgb.z, alpha);
	}
}
