#pragma once
#include <vector>
#include <string>
#include <SimpleMath.h>

//ノーツが流れてくる方向
enum class LaneDir { Back, Top, Right, Left};

//レーンの区切り：「この小節からこの方向に」
struct TrackSection
{
	int fromMeasure = 1;	//この小節から
	LaneDir dir = LaneDir::Back;	//この方向に
	bool hasJudge = false;	//判定円の位置が書いてあるか
	DirectX::SimpleMath::Vector3 judge;	//この区間の判定円の位置
};

//レーン一本。今はキーとレーンが同じ番号に
struct LaneTrack
{
	int key = 0;	//このレーンのキー番号
	std::vector<TrackSection> sections;	//fromMeasureの小さい順

	//measure小節目の設定（区切りがなければ奥）
	TrackSection SectionAt(int measure) const
	{
		TrackSection s;
		for (const auto& sec : sections)
		{
			if (sec.fromMeasure <= measure) s = sec;
			else break;
		}
		return s;
	}
};

namespace LaneDirUtil
{
	inline LaneDir FromString(const std::string& s)
	{
		if (s == "top") return LaneDir::Top;
		if (s == "right") return LaneDir::Right;
		if (s == "left") return LaneDir::Left;
		return LaneDir::Back;
	}

	//判定円から見て出現位置をどれだけずらすか（奥からは今まで通りなので使わない）
	inline DirectX::SimpleMath::Vector3 SpawnOffset(LaneDir d)
	{
		switch (d)
		{
		case LaneDir::Top: return DirectX::SimpleMath::Vector3(0.0f, 535.0f, 150.0f);
		case LaneDir::Right: return DirectX::SimpleMath::Vector3(900.0f, 200.0f, 0.0f);
		case LaneDir::Left: return DirectX::SimpleMath::Vector3(-900.0f, 0.0f, 0.0f);
		case LaneDir::Back: return DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
		}
	}

	//向きごとのノーツの回し方
	inline DirectX::SimpleMath::Matrix NoteRotation(LaneDir d)
	{
		using DirectX::SimpleMath::Matrix;
		switch (d)
		{
		case LaneDir::Top:	//起こして面をカメラへ向ける
			return Matrix::CreateRotationX(-DirectX::XM_PIDIV2);
		case LaneDir::Right:
		case LaneDir::Left:	//起こしてさらに90度回転
			return Matrix::CreateRotationX(-DirectX::XM_PIDIV2) * Matrix::CreateRotationZ(DirectX::XM_PIDIV2);
		default:
			return Matrix::Identity;
		}
	}

	//判定円が並ぶ向き（横からくるレーンは縦に）
	inline DirectX::SimpleMath::Vector3 Lateral(LaneDir d)
	{
		if (d == LaneDir::Right || d == LaneDir::Left) return DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);
		return DirectX::SimpleMath::Vector3(1.0f, 0.0f, 0.0f);
	}

	//キーの画像を判定円からどれだけずらすか
	inline DirectX::SimpleMath::Vector2 IconOffset(LaneDir d)
	{
		switch (d)
		{
		case LaneDir::Right: return DirectX::SimpleMath::Vector2(-90.0f, 0.0f);	//右から来る->画像は左
		case LaneDir::Left:  return DirectX::SimpleMath::Vector2(90.0f, 0.0f);	//左から来る->画像は右
		case LaneDir::Top:   return DirectX::SimpleMath::Vector2(0.0f, -90.0f);	//上から来る->画像は下
		default:             return DirectX::SimpleMath::Vector2(0.0f, 0.0f);
		}
	}
}


