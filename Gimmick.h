#pragma once
#include <string>
#include <vector>

//譜面の演出を司ります。追加はここまで
//演出を足すときはGimmickTypeに足す->ChartLoaderで読む->ここで計算の流れ

enum class GimmickType
{
	Unknown,	//まだ使ってない種類
	Speed,		//速さ：1=等速　0=停止　-1=逆走
};

struct GimmickEvent
{
	GimmickType type = GimmickType::Unknown;
	float start = 0.0f;		//始まり
	float end = 0.0f;		//終わり
	float easeIn = 0.0f;	//入りにかける秒
	float easeOut = 0.0f;	//抜けにかける秒
	float speed = 1.0f;

	float Weight(float t) const;
};

namespace Gimmick
{
	//判定円より手前（逆走で下がってから上がってくるところ）に、流れの長さの何割まで見せるか
	constexpr float kBelowLimit = 0.35f;

	//譜面を読んだ時に呼ぶ　songLengthは表を作る長さを秒で
	void Load(const std::vector<GimmickEvent>& events, float songLength);
	void Clear();

	//t秒までにノーツが進んだ量　演出があったときのため
	float Scroll(float t);
	//t秒の速さ
	float SpeedAt(float t);

	//出現～判定を0~1とした今の進み具合。クランプはしない
	//0：未満　奥　１判定　１＜判定円より手前（逆走の時だけ）
	float Progress(float spawnTime, float arriveTime, float now);

	//見えるか（演出時だけ）
	bool InView(float spawnTime, float arriveTime, float now);
	bool PointVisible(float tau, float now, float approach);

	//速さの演出があるか
	bool HasSpeed();

	const std::vector<GimmickEvent>& Events();
}

