#pragma once
// ============================================================
// StageBackdrop : ステージの一番奥に、画面いっぱいの背景画像を1枚貼るだけのオブジェクト。
//   SkyBoxの代わりに使う。深度を一切使わずに描くので、
//   後から描く敵・ノーツ・スポットライトなどは必ずこの手前に見える。
//   ※シーンに AddObject する順番は「一番最初」にすること（先に描いたものが奥になる）。
// ============================================================
#include "Object.h"
#include "Texture2D.h"

class StageBackdrop : public Object
{
public:
	void Init() override;
	void Update() override {}
	void Draw(Camera* cam) override;
	void Uninit() override {}

	// 画像を差し替えたいとき用（Init後に呼ぶ）
	void SetImage(const char* path) { m_Image.SetTexture(path); }

private:
	Texture2D m_Image;
};
