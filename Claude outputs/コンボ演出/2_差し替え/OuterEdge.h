#pragma once
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include <SimpleMath.h>
#include <vector>
#include <chrono>

// ============================================================
// OuterEdge : レーンの外側（客席）に並ぶ、観客のペンライト
//   ・盛り上がり（SetHype 0～1）が上がるほど、ペンライトの数が増え、明るく、色とりどりになる
//   ・拍に合わせて左右に振る（拍ちょうどで左右の端に来る）
//   ・盛り上がるほど、全員の振りがそろっていく
//   ・拍の頭で一瞬明るくなる
//   描き方は SpotlightRow と同じ（spotlightPS の「光の柱」と「丸い光」）
// ============================================================
class OuterEdge : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override {}

	// 列の奥の端（敵の位置を渡す。今は使っていない）
	void SetFarTarget(const DirectX::SimpleMath::Vector3& pos) { m_FarTarget = pos; }

	// 曲の再生時刻(秒)とBPM。渡されている間は拍に合わせて振る
	void SetBeat(float musicTime, float bpm) { m_MusicTime = musicTime; m_Bpm = bpm; m_HasBeat = true; }

	// 盛り上がり 0～1（ChartObject::GetHype01 を渡す）
	void SetHype(float hype01) { m_Hype = (hype01 < 0.0f) ? 0.0f : (hype01 > 1.0f ? 1.0f : hype01); }

	// ---- 調整用 ----
	static constexpr int   kCount = 80;				// ペンライトの数（全部出た時）
	static constexpr float kMinVisible = 0.3f;		// 盛り上がり 0 の時に出ている割合
	static constexpr float kMinBright = 0.35f;		// 盛り上がり 0 の時の明るさ
	static constexpr float kBeatFlash = 0.5f;		// 拍の頭で明るくなる量（盛り上がるほど強く）
	static constexpr float kSwingDeg = 25.0f;		// 左右に振る角度
	static constexpr float kStickLength = 60.0f;	// 棒の長さ
	static constexpr float kStickWidth = 10.0f;		// 棒の太さ
	static constexpr float kTipSize = 24.0f;		// 先の丸い光の大きさ

private:
	struct Penlight
	{
		DirectX::SimpleMath::Vector3 pos;	// 持ち手の位置
		float size;			// 大きさの倍率
		float phase;		// 上下にゆれる位相
		float speed;		// 上下にゆれる速さ
		float order;		// 出てくる順番（0～1。盛り上がりがこれを超えたら出る）
		float colorPick;	// 色が変わる順番（0～1。盛り上がりがこれを超えたら色とりどりの色になる）
		int   colorIndex;	// 色とりどりの時の色
		float swingJitter;	// 振りのずれ（拍の数。盛り上がるほど 0 に近づく）
	};
	struct Params
	{
		DirectX::SimpleMath::Vector4 color;	// rgb=色, a=明るさ
		float mode;				// 0=光の柱, 1=丸い光
		float baseWidthRatio;	// 柱の根本の幅 / 先端の幅
		float padding[2];		// 16バイト境界合わせ
	};	// sizeof == 32

	void DrawQuad(const DirectX::SimpleMath::Matrix& world, const Params& params);

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	std::vector<Penlight> m_Lights;

	DirectX::SimpleMath::Vector3 m_FarTarget = DirectX::SimpleMath::Vector3(0.0f, 100.0f, 200.0f);

	float m_MusicTime = 0.0f;
	float m_Bpm = 0.0f;
	bool  m_HasBeat = false;
	float m_Hype = 0.0f;
	float m_Time = 0.0f;
	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;
};
