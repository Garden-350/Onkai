#pragma once
// ============================================================
// Gimmick : 譜面の演出（ギミック）
//   いまは「速さ」だけ：低速・止める・速くする・逆走（speed がマイナス）
//   譜面の JSON の "gimmicks" を読む。演出は見た目だけで、判定の時刻は変えない。
//
//   考え方：「進んだ量」Scroll(t)
//     普通は 1 秒で 1 秒ぶん進む（Scroll(t) = t）。速さ 0.4 なら 1 秒で 0.4 秒ぶん、
//     速さ -1 なら 1 秒で 1 秒ぶん戻る。ノーツの距離は「そのノーツの目盛り − 今の目盛り」。
//     叩く時刻には目盛りの差が必ず 0 になるので、判定円の上にちゃんといる。
//
//   演出を足すとき：GimmickType に足す → ChartLoader で読む → ここで計算
// ============================================================
#include <string>
#include <vector>

enum class GimmickType
{
	Unknown,	// まだ使っていない種類（読み飛ばす）
	Speed,		// 速さ：1=普通 0.4=4割 0=止まる -1=同じ速さで逆
	Rise,		// 下から戻る：始まりの瞬間、見えているノーツが判定円の下に集まり、勢いよく元の位置へ戻る。その後は普通に落ちてくる
};

struct GimmickEvent
{
	GimmickType type = GimmickType::Unknown;
	float start = 0.0f;		// 始まり（秒）
	float end = 0.0f;		// 終わり（秒）
	float easeIn = 0.0f;	// 入りにかける秒（譜面には拍で書いてあるのを、読むときに秒へ直す）
	float easeOut = 0.0f;	// 抜けにかける秒
	float speed = 1.0f;		// Speed：速さ
	float from = -0.3f;		// Rise：始まりの瞬間の「距離の倍率」（-0.3 = 判定円の少し下。0 = 判定円の所）。end までに 1（元の位置）へ戻る

	// 0〜1：区間の外は 0、中は 1、入り・抜けはなめらかに
	float Weight(float t) const;
};

namespace Gimmick
{
	// 判定円より手前（逆走で下から上がってくる所）に、流れの長さの何割まで見せるか
	constexpr float kBelowLimit = 0.35f;

	// 譜面を読んだ時に呼ぶ。songLength は表を作る長さ（秒）
	void Load(const std::vector<GimmickEvent>& events, float songLength);
	void Clear();

	// t 秒までにノーツが進んだ量（演出が無ければ t と同じ）
	float Scroll(float t);
	// t 秒の速さ
	float SpeedAt(float t);

	// 出現〜判定 を 0〜1 とした、今の進み具合（クランプしない）
	//   0 未満 = まだ奥（見えない）、1 = 判定円、1 より大きい = 判定円より手前（逆走の時だけ）
	float Progress(float spawnTime, float arriveTime, float now);

	// 見た目の進み具合：Progress に「下から戻る」をかけたもの。ノーツを描く場所はこっちで出す
	//   判定円からの距離 (1 - Progress) を DisplayScale 倍する。判定円の所（距離 0）は動かないので、叩く時刻の見た目はずれない
	float DisplayProgress(float spawnTime, float arriveTime, float now);
	// 今の「距離の倍率」（下から戻る の間だけ 1 以外。演出が無ければ 1）
	float DisplayScale(float now);

	// 見えるか（今までの「出現時刻 <= 今 <= 判定時刻」の代わり。演出が無ければ同じ結果）
	bool InView(float spawnTime, float arriveTime, float now);

	// tau 秒に判定円へ届く所（ロングの帯の 1 点など）が、今見えるか。approach = 奥の端から判定円までの秒数
	//   ・判定円より手前（下）は、逆走で上がってきている間だけ見せる
	//   ・この先の逆走で「一度判定円を通り過ぎてしまう」ノーツは、逆走が始まるまで隠す
	//     （逆走の後に来るノーツが、逆走の前に判定円を素通りして見えないように）
	bool PointVisible(float tau, float now, float approach);

	// 速さの演出があるか（無ければ今までと同じ計算でよい）
	bool HasSpeed();

	const std::vector<GimmickEvent>& Events();
}
