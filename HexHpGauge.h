#pragma once
#include "Texture2D.h"

// ============================================================
// HexHpGauge : 敵の六角形HPゲージ（画面に貼る2D表示）
//   ・一番上の頂点から反時計回りに削れていく（36段階のスプライトシート）
//   ・減った直後は白い残像が残り、少し遅れて追いかけて減る（格闘ゲーム風）
//   ・ダメージを受けた瞬間、少しだけ膨らむ
//   使う画像（assets/UI/）：
//     hp_hex_frame.png        枠（常に表示）
//     hp_hex_fill_sheet.png   赤いゲージ本体（6x6=36コマ）
//     hp_hex_trail_sheet.png  白い残像（並びは本体と同じ）
// ============================================================
class HexHpGauge
{
public:
	void Init();
	void SetPosition(float x, float y) { m_X = x; m_Y = y; }	// 画面座標（中心が0、上がプラス）
	void SetSize(float size) { m_Size = size; }				// 表示の大きさ（正方形の一辺）
	void SetRatio(float ratio);		// 0～1。毎フレーム呼んでよい（減った時だけ演出が入る）
	void Update(float dt);
	void Draw(Camera* cam);

private:
	static constexpr int kSteps = 36;	// シートのコマ数
	static constexpr int kCols = 6;		// シートの横のコマ数（縦も6）
	static constexpr float kTrailDelay = 0.35f;	// 残像が減り始めるまでの秒数
	static constexpr float kTrailSpeed = 0.5f;	// 残像が1秒にどれだけ減るか（割合）

	static void SetStep(Texture2D& tex, float ratio);	// 割合 → シートのコマ

	Texture2D m_Frame;
	Texture2D m_Trail;
	Texture2D m_Fill;
	float m_Ratio = 1.0f;		// 今のHP（0～1）
	float m_TrailRatio = 1.0f;	// 残像のHP（遅れて追いかける）
	float m_TrailWait = 0.0f;	// 残像が減り始めるまでの残り時間
	float m_HitPop = 0.0f;		// ダメージ直後の膨らみ（1→0）
	float m_X = 0.0f;
	float m_Y = 270.0f;
	float m_Size = 150.0f;
};
