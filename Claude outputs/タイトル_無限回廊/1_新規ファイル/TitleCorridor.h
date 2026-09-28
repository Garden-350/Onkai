#pragma once
// ============================================================
// TitleCorridor : タイトル画面の背景。「どこまで歩いても出口にたどり着けない回廊」。
//   ・カメラは前へ歩き続けていて、床・壁・天井の線が手前へ流れてくる。
//     なのに奥の「出口」の扉だけは距離が変わらない → 永遠にたどり着けない。
//   ・StartExit() を呼ぶと、扉が一瞬ためてから一気に迫ってきて、光があふれる。
//     IsExitFinished() が true になったらシーンを切り替えてよい。
//   ・カメラの位置もこのクラスが毎フレーム決める（SetFixedPose）。
//   ・マス目の線はシェーダー(titleCorridorPS.hlsl)で計算。扉の絵だけテクスチャを使う。
//   ※シーンに AddObject する順番は「一番最初」にすること（一番奥に描くため）。
// ============================================================
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"
#include "Renderer.h"
#include <SimpleMath.h>
#include <chrono>

class TitleCorridor : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override {}

	void StartExit();									// Enterが押されたら呼ぶ（2回目以降は無視）
	bool IsExiting() const { return m_Exiting; }
	bool IsExitFinished() const { return m_ExitFinished; }

	float GetExitProgress() const;	// 出口演出の進み具合 0～1
	float GetIntroFade() const;		// 起動時の黒フェードの濃さ 1(真っ黒)→0
	float GetExitWhite() const;		// 出口演出の白フェードの濃さ 0→1(真っ白)

private:
	struct CorridorParams
	{
		DirectX::SimpleMath::Vector4 lineColor;	// rgb=線の色, a=線の明るさ
		DirectX::SimpleMath::Vector4 fillColor;	// rgb=面の色
		DirectX::SimpleMath::Vector4 fogColor;	// rgb=霧の色, a=霧の最大の濃さ
		DirectX::SimpleMath::Vector4 doorColor;	// rgb=扉からもれる光の色, a=その強さ
		float mode;			// 0=床・壁・天井 / 1=奥の壁 / 2=扉
		float sizeU;		// 板の横の長さ
		float sizeV;		// 板の縦の長さ
		float zNear;		// 板の手前の端のz
		float scroll;		// 歩いた距離
		float cell;			// マス目の大きさ
		float lineWidth;	// 線の太さ
		float zDoor;		// 扉のz
		float fogStart;
		float fogEnd;
		float doorW;
		float doorH;
		float openAmount;	// 扉から光があふれる量 0～1
		float revealDist;	// この距離より手前の線だけ光る
		float padding0;
		float padding1;
	};	// sizeof == 128（titleCorridorPS.hlsl と同じ並び）

	// center を中心に、axisU(横)・axisV(縦)の長さの板を描く。uv.y=0 が +axisV 側
	void DrawBasis(const DirectX::SimpleMath::Vector3& center,
		const DirectX::SimpleMath::Vector3& axisU, const DirectX::SimpleMath::Vector3& axisV,
		const CorridorParams& params);
	void UpdateCamera();

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	Texture m_DoorTexture;

	// ── 状態 ──
	std::chrono::steady_clock::time_point m_LastClock;
	float m_Time = 0.0f;		// シーンが始まってからの秒数
	float m_Scroll = 0.0f;		// 歩いた距離
	float m_DoorZ = 0.0f;		// 今の扉のz
	bool  m_Exiting = false;
	bool  m_ExitFinished = false;
	float m_ExitTimer = 0.0f;

	// ── 回廊の形（見た目の調整用）──
	float m_Width = 500.0f;			// 回廊の幅（マス目の倍数にすると角に線が来る）
	float m_Height = 400.0f;		// 回廊の高さ（床 y=0 ～ 天井 y=m_Height）
	float m_NearZ = -20.0f;			// 回廊の手前の端（カメラは z=0）
	float m_Cell = 100.0f;			// マス目の大きさ
	float m_LineWidth = 3.0f;		// 線の太さ

	float m_DoorDistance = 1100.0f;	// 扉までの距離（Enterを押すまでずっとこの距離）
	float m_DoorWidth = 240.0f;		// 扉の幅
	float m_DoorHeight = 360.0f;	// 扉の高さ（扉の絵は 2:3 で作ってある）

	// ── カメラ ──
	float m_EyeHeight = 190.0f;		// 目の高さ
	float m_LookDownDeg = 6.0f;		// 少し見下ろす角度（大きくすると扉が画面の上に寄る）
	float m_WalkSpeed = 120.0f;		// 歩く速さ（1秒あたり）
	float m_BobAmount = 3.0f;		// 歩くときの上下の揺れ

	// ── 色・霧 ──
	DirectX::SimpleMath::Vector3 m_LineColor = DirectX::SimpleMath::Vector3(0.45f, 0.85f, 1.0f);
	float m_LineIntensity = 1.0f;
	DirectX::SimpleMath::Vector3 m_FillColor = DirectX::SimpleMath::Vector3(0.015f, 0.02f, 0.045f);
	DirectX::SimpleMath::Vector3 m_FogColor = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	float m_FogStart = 150.0f;
	float m_FogEnd = 1300.0f;
	float m_FogMax = 0.9f;
	DirectX::SimpleMath::Vector3 m_DoorLightColor = DirectX::SimpleMath::Vector3(0.55f, 1.0f, 0.7f);
	float m_DoorLightIntensity = 0.8f;

	// ── 演出の時間 ──
	float m_IntroSeconds = 1.2f;		// 起動時、黒から明けるまで
	float m_ExitSeconds = 1.6f;			// Enterから扉に着くまで
	float m_ExitHoldSeconds = 0.2f;		// 真っ白になってから切り替えるまで
	float m_ExitAnticipation = 0.8f;	// 迫る前に一瞬うしろへ引く量（0で引かない）
	float m_ExitDoorZ = 40.0f;			// 扉が最後に来る位置（カメラの目の前）
};
