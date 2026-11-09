#pragma once
#include <vector>
#include <string>
#include <SimpleMath.h>

//カメラの位置と見る先
struct CameraPose
{
	DirectX::SimpleMath::Vector3 eye;	//カメラの位置
	DirectX::SimpleMath::Vector3 target;	//見る先
};

//カメラの区切り　何小節目からこのカメラになるか
struct CameraKey
{
	int fromMeasure = 1;
	std::string preset = "default";	//カメラの名前(下の表に記述あり)
	float blendBeats = 2.0f;	//何拍かけて動くか（小節の終わりで終わる）
};

namespace CameraPresets
{
	//名前->カメラの位置。defaultはstage1sceneが決める
	//新しいカメラがほしい場合はここに記述したらjsonが読む
	inline bool Find(const std::string& name, CameraPose& out)
	{
		using DirectX::SimpleMath::Vector3;
		struct Preset { const char* name; Vector3 eye; Vector3 target; };
		//判定円の真ん中が (0, -480, -210) の時に合わせた値
		static const Preset kPresets[] =
		{
			{ "front",   Vector3(   0.0f,  150.0f, -2000.0f), Vector3(  0.0f,  150.0f,   0.0f) },	//真正面（横・上から来るレーン用）
			{ "low",     Vector3(   0.0f,  250.0f, -1250.0f), Vector3(  0.0f, -430.0f, 450.0f) },	//低い所から　奥が遠くまで見える
			{ "far",     Vector3(   0.0f,  850.0f, -1350.0f), Vector3(  0.0f, -480.0f, 350.0f) },	//少し引いた
			{ "high",    Vector3(   0.0f, 1300.0f,  -800.0f), Vector3(  0.0f, -480.0f, 450.0f) },	//高い所から見下ろす
			{ "right",   Vector3( 520.0f,  560.0f,  -900.0f), Vector3( 60.0f, -480.0f, 350.0f) },	//右から
			{ "left",    Vector3(-520.0f,  560.0f,  -900.0f), Vector3(-60.0f, -480.0f, 350.0f) },	//左から
			{ "rightup", Vector3( 420.0f, 1300.0f,  -900.0f), Vector3( 40.0f, -480.0f, 400.0f) },	//右上から（rightとhighを混ぜた）
			{ "leftup",  Vector3(-420.0f, 1300.0f,  -900.0f), Vector3(-40.0f, -480.0f, 400.0f) },	//左上から
		};
		for (const auto& p : kPresets)
		{
			if (name == p.name)
			{
				out.eye = p.eye;
				out.target = p.target;
				return true;
			}
		}
		return false;	//知らない名前は"default"
	}
}

class CameraTrack
{
};

