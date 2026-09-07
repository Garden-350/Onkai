#pragma once
#include <SimpleMath.h>

// ============================================================
// ホーミングビーム1発分のデータと実行時状態
//   （ChartObject.h から HomingBeamSystem との共有のため切り出し）
// ============================================================
struct ChartHomingBeam
{
	float chargeTime = 0.0f;	// 探索(タンタンタン)を開始する時刻
	float lockTime = 0.0f;		// 発射する時刻
	float hitTime = 0.0f;		// 命中判定(ダメージ/ドッジ確定)を打ち切る時刻

	// ビームの進行段階
	enum class State
	{
		Homing,		// 探索中〜発射待ち。プレイヤーを追いかけながらバラけた線を表示
		Fired,		// 発射済み。命中/回避を一定間隔でチェック中
		Resolved	// 判定終了。以後何もしない
	};
	State state = State::Homing;

	bool judgmentLocked = false;			// ロック位置が確定したか
	int  lockedSide = 0;					// ロック時のプレイヤー位置(-1/0/1)
	DirectX::SimpleMath::Vector3 lockedPos{};	// ロックされた着弾座標

	bool lockSoundPlayed = false;	// ロック音を鳴らしたか
	bool fireSoundPlayed = false;	// 発射音を鳴らしたか
	int  searchBeatsPlayed = 0;	// 「タン」を何回鳴らしたか
	float nextCountTime = 0.0f;	// 次に命中判定を行う時刻

	static constexpr int   kSearchBeatCount = 3;	// 「タン」を鳴らす回数
	static constexpr float kCountInterval = 0.5f;	// 発射後、何秒おきに命中判定するか
	static constexpr int   kScatterCount = 6;		// 探索中に表示するバラけた線の本数
	DirectX::SimpleMath::Vector3 scatterOffsets[kScatterCount]; // 各線のランダムな初期オフセット
};

struct BeamShaderParms
{
	float time;
	float intensity;
	float padding0[2];
	DirectX::SimpleMath::Vector4 baseColor;
};	//sizeof == 32 (16の倍数ならOK)
