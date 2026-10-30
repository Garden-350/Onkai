#pragma once
// SpotlightRow : ランウェイ風のスポットライト列。
//   画面下の左右の端から、敵の足元に向かって一定間隔でライトを並べ、
//   それぞれ真上（少し外側に傾けて）へ光の柱を伸ばす。
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include <SimpleMath.h>
#include <chrono>

class SpotlightRow : public Object
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
	void SetHype(float hype01) { m_Hype = hype01; }
private:
	struct SpotlightParams
	{
		DirectX::SimpleMath::Vector4 color;	// rgb=色, a=明るさ
		int mode;				// 0=光の柱, 1=根本のランプ
		float baseWidthRatio;	// mode0用：根本の幅 / 先端の幅
		float padding[2];		// 16バイト境界合わせ
	};	// sizeof == 32

	void ComputeNearFromScreen(Camera* cam);
	void DrawQuad(const DirectX::SimpleMath::Matrix& world, const SpotlightParams& params);

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	DirectX::SimpleMath::Vector3 m_FarTarget = DirectX::SimpleMath::Vector3(0.0f, 100.0f, 200.0f);
	DirectX::SimpleMath::Vector3 m_NearLeft = DirectX::SimpleMath::Vector3(-520.0f, -140.0f, -600.0f);	// 自動計算できなかったときの予備値
	DirectX::SimpleMath::Vector3 m_NearRight = DirectX::SimpleMath::Vector3(520.0f, -140.0f, -600.0f);
	bool m_NearComputed = false;

	float m_MusicTime = 0.0f;
	float m_Bpm = 0.0f;
	float m_Hype = 1.0f;	//盛り上がり部分
	bool  m_HasBeat = false;
	float m_Time = 0.0f;	// 自前の経過時間（ゆらゆら揺らす用）
	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;

	//見た目の調整用
	int   m_LightsPerSide = 8;		// 片側のライトの数
	float m_BaseY = -140.0f;		// ライトを置く床の高さ（判定ライン y=-55 より少し下）
	float m_FarHalfWidth = 330.0f;	// 奥の端での、列の中心から左右までの距離
	float m_ScreenEdgeX = 0.92f;	// 手前の端をどれだけ画面の端に寄せるか(0〜1。1で画面ぴったり端)
	float m_ScreenEdgeY = -0.92f;	// 手前の端の画面上の高さ(-1で画面の一番下)
	float m_BeamLength = 1100.0f;	// 光の柱の長さ
	float m_BaseWidth = 22.0f;		// 光の柱の根本の太さ
	float m_TopWidth = 170.0f;		// 光の柱の先端の太さ
	float m_ScreenUpBlend = 1.0f;	// 光の向き。1=画面の上へまっすぐ / 0=ワールドの真上（見下ろしカメラだと手前に倒れて見える）
	float m_LeanDeg = 8.0f;			// 外側への傾き(度)。0でまっすぐ。レーンの上に光が被らないよう外へ逃がす
	float m_SwayDeg = 4.0f;			// ゆらゆら揺れる幅(度)
	float m_BaseIntensity = 0.30f;	// 普段の明るさ
	float m_PulseIntensity = 0.70f;	// 拍に合わせて走る光の明るさ
	float m_LampSize = 80.0f;		// 根本のランプの大きさ
};
