#pragma once
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include <SimpleMath.h>
#include <chrono>

class OuterEdge : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override {}
	// 列の奥の端（敵の位置を渡す。x=列の中心, z=奥の端の奥行き）
	void SetFarTarget(const DirectX::SimpleMath::Vector3& pos) { m_FarTarget = pos; }

	// 曲の再生時刻(秒)とBPM。渡されている間は拍に合わせて光が走る
	void SetBeat(float musicTime, float bpm) { m_MusicTime = musicTime; m_Bpm = bpm; m_HasBeat = true; }

	//盛り上がり0~1
	void SetHype(float hype01) { m_Hype = (hype01 < 0.0f) ? 0.0f : (hype01 > 1.0f ? 1.0f : hype01); }

	//調整用
	static constexpr int kCount = 80;	//ペンライトの数
	static constexpr float kMinVisible = 0.3f;	//盛り上がり０の時に出ている割合
	static constexpr float kMinBright = 0.35f;	//盛り上がり０の時の明るさ
	static constexpr float kBeatFlash = 0.5f;	//拍の頭で明るくなる量
	static constexpr float kSwingDeg = 25.0f;	//左右に振る角度
	static constexpr float kStickLength = 60.0f;	//棒の長さ
	static constexpr float kStickWidth = 10.0f;	//棒の太さ
	static constexpr float kTipSize = 24.0f;	//先の丸い光の大きさ
private:
	struct Penlight
	{
		DirectX::SimpleMath::Vector3 pos;	//持ち手の位置
		float size;	//上下に揺れる位相
		float phase;	//上下に揺れる速さ
		float speed;	//速さ
		float order;	//出てくる順番（０～１盛り上がりがこれを超えたら出るように）
		float colorPick;//色が変わる順番
		int colorIndex;	//色とりどりの時の色
		float swingJitter;	//振りのずれ
	};
	struct Params
	{
		DirectX::SimpleMath::Vector4 color;	//rgb=color, a=alpha
		float mode;	//0=光の柱、1=丸い光
		float baseWidthRatio;	//柱の根元に幅　/　先端の幅
		float padding[2];	//16バイト境界合わせ
	};

	void DrawQuad(const DirectX::SimpleMath::Matrix& world, const Params& params);

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	std::vector<Penlight> m_Lights;

	DirectX::SimpleMath::Vector3 m_FarTarget = DirectX::SimpleMath::Vector3(0.0f, 100.0f, 200.0f);

	std::chrono::steady_clock::time_point m_LastClock;
	float m_MusicTime = 0.0f;
	float m_Bpm = 0.0f;
	float m_Hype = 0.0f;
	float m_Time = 0.0f;	// 自前の経過時間（ゆらゆら揺らす用必要なら）
	bool  m_HasBeat = false;
	bool m_ClockStarted = false;
};

