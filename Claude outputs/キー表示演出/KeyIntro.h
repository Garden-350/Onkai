#pragma once
#include "Texture2D.h"
#include <SimpleMath.h>

class Camera;

// ============================================================
// KeyIntro : ステージの始めに、S D F J K L のキーを
//   左のキーから順に、下からスッと上げて見せ、少し待ってから消す
//   ・判定円の少し下に止まる（どのキーがどのレーンか分かるように）
//   ・出てくる時は 透明 → 半透明、消える時は 半透明 → 透明
//   ・時間だけで決まるので、持っている状態は無い（リトライしても最初から）
// ============================================================
class KeyIntro
{
public:
	void Init();

	// t         : ステージが始まってからの秒数（カウントインの頭が 0）
	// screenPos : 各レーンの判定円の画面の位置（ChartObject::Draw で計算済みのもの）
	void Draw(Camera* cam, float t, const DirectX::SimpleMath::Vector3 screenPos[6]);

	// ---- 調整用 ----
	static constexpr float kStartDelay = 0.2f;	// シーンが始まってから最初のキーが出るまで
	static constexpr float kStagger = 0.08f;	// 隣のキーが出るまでのずれ（左から順に）
	static constexpr float kRiseTime = 0.35f;	// 上がり切るまで
	static constexpr float kRiseDist = 120.0f;	// どれだけ下から上がってくるか
	static constexpr float kHoldTime = 1.2f;	// 上がり切ってから消え始めるまで
	static constexpr float kFadeTime = 0.4f;	// 消えるまで
	static constexpr float kPeakAlpha = 0.8f;	// 一番濃い時の濃さ（半透明）
	static constexpr float kTargetDY = -80.0f;	// 判定円からどれだけ下に止めるか（マイナスで下）
	static constexpr float kSize = 70.0f;		// キーの画像の大きさ

private:
	Texture2D m_Keys[6];
};
