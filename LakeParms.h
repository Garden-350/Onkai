#pragma once

#include <SimpleMath.h>
#include <cmath>

struct LakeParms
{
	DirectX::SimpleMath::Vector4 camPos;		// xyz=カメラの位置, w=経過秒
	DirectX::SimpleMath::Vector4 camForward;	// xyz=カメラの前, w=tan(縦の視野角/2)
	DirectX::SimpleMath::Vector4 camRight;		// xyz=カメラの右, w=画面の横/縦
	DirectX::SimpleMath::Vector4 camUp;			// xyz=カメラの上, w=露出
	DirectX::SimpleMath::Vector4 sunDir;		// xyz=太陽のある方向, w=太陽の円盤の見え具合
	DirectX::SimpleMath::Vector4 sunColor;		// rgb=太陽の光, a=夜の度合い(0=昼 1=夜)
	DirectX::SimpleMath::Vector4 moonDir;		// xyz=月のある方向, w=月の光の強さ
	DirectX::SimpleMath::Vector4 skyZenith;		// rgb=真上の空の色, a=星の量
	DirectX::SimpleMath::Vector4 skyHorizon;	// rgb=地平線の空の色, a=雲の量
	DirectX::SimpleMath::Vector4 ambientCol;	// rgb=空からの光, a=霧の濃さ
	DirectX::SimpleMath::Vector4 wind;			// xy=風が吹いていく向き(xz平面), z=ふだんの風の強さ, w=突風の強さ
	DirectX::SimpleMath::Vector4 misc;			// x=突風の先頭の位置, y=手前の岸までの距離, z=飛び込みの進み具合
};	// sizeof == 192

namespace LakeShape
{
	constexpr float kBankY = 8.0f;	//手前の岸の高さ。水面はy=0

	//手前の岸の線
	inline float ShoreZ(float x, float baseZ)
	{
		return baseZ + 40.0f * sinf(x * 0.004f + 1.3f) + 18.0f * sinf(x * 0.013f + 0.4f);
	}
}