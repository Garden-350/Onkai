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

private:
	struct Orb
	{
		DirectX::SimpleMath::Vector3 pos;
		DirectX::SimpleMath::Vector3 color;
		float size;
		float phase;
		float speed;
	};
	std::vector<Orb> m_Orbs;
	struct OuterEdgeParams
	{
		DirectX::SimpleMath::Vector4 color;	// rgb=色, a=明るさ
		float mode;				// 0=光の柱, 1=根本のランプ
		float baseWidthRatio;	// mode0用：根本の幅 / 先端の幅
		float padding[2];		// 16バイト境界合わせ
	};	// sizeof == 32

	void ComputeNearFromScreen(Camera* cam);
	void DrawQuad(const DirectX::SimpleMath::Matrix& world, const OuterEdgeParams& params);

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	DirectX::SimpleMath::Vector3 m_FarTarget = DirectX::SimpleMath::Vector3(0.0f, 100.0f, 200.0f);
	DirectX::SimpleMath::Vector3 m_NearLeft = DirectX::SimpleMath::Vector3(-520.0f, -140.0f, -600.0f);	// 自動計算できなかったときの予備値
	DirectX::SimpleMath::Vector3 m_NearRight = DirectX::SimpleMath::Vector3(520.0f, -140.0f, -600.0f);
	bool m_NearComputed = false;

	float m_MusicTime = 0.0f;
	float m_Bpm = 0.0f;
	bool  m_HasBeat = false;
	float m_Time = 0.0f;	// 自前の経過時間（ゆらゆら揺らす用必要なら）
	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;

	//見た目の調整用
	int   m_LightsPerSide = 80;		// 片側のライトの数
};

