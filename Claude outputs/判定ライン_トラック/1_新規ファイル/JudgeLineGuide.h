#pragma once
// ============================================================
// JudgeLineGuide : ビートセーバー風の「打つ場所」と「あと何拍か」を常に見せるガイド。
//   1) ストライクライン：6つの判定円を光るラインでつなぎ、各判定円に光るパッドを置く。
//      キーを押している間・判定した瞬間はそのレーンのパッドが明るくなる。
//   2) レーンのトラック：各判定円から、ノーツが飛んでくる方向へ床の帯を伸ばす。
//      その上を拍ごとの線が奥から判定ラインへ流れてくる（小節の頭は太く明るい）。
//      線はノーツと同じ速さで進むので、「ノーツがどの線と一緒に来ているか」でタイミングが読める。
//
//   判定円が動く譜面でも、毎フレーム渡された判定円の位置で描き直すので追従する。
//   テクスチャ不要（形と光り方は judgeGuidePS.hlsl で計算する）。
//   ※加算合成・深度は書かないので、ノーツより「先に」描くこと。
// ============================================================
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include <SimpleMath.h>

class Camera;

class JudgeLineGuide
{
public:
	static constexpr int kLaneCount = 6;

	// 毎フレーム ChartObject から渡してもらう情報
	struct Frame
	{
		DirectX::SimpleMath::Vector3 judgePos[kLaneCount];		// 今の判定円の位置
		DirectX::SimpleMath::Vector3 trackOffset[kLaneCount];	// 判定円 → トラックの奥の端 までのベクトル（ハイスピ込み）
		DirectX::SimpleMath::Vector3 laneColor[kLaneCount];
		float laneGlow[kLaneCount];		// 0〜1。キーを押している/判定した直後ほど大きい
		float musicTime;				// 曲の再生時刻(秒)
		float bpm;
		int   beatsPerBar;				// 何拍ごとに太い線(小節線)にするか
		float approachSeconds;			// ノーツがトラックの奥から判定円まで来るのにかかる秒数
	};

	void Init();
	void Draw(Camera* cam, const Frame& f);

private:
	struct GuideParams
	{
		DirectX::SimpleMath::Vector4 color;	// rgb=色, a=明るさ
		float mode;		// 0=ライン, 1=トラック, 2=パッド
		float param1;	// mode0:両端のフェード幅 / mode2:押し込み具合
		float param2;
		float padding;
	};	// sizeof == 32

	// center を中心に、axisU(横幅ぶん)・axisV(縦の長さぶん)の板を描く。uv.y=0 が +axisV 側
	void DrawBasis(const DirectX::SimpleMath::Vector3& center,
		const DirectX::SimpleMath::Vector3& axisU, const DirectX::SimpleMath::Vector3& axisV,
		const GuideParams& params);
	// from→to を結ぶ、カメラの方を向いた帯
	void DrawRibbon(const DirectX::SimpleMath::Vector3& from, const DirectX::SimpleMath::Vector3& to,
		float width, const DirectX::SimpleMath::Vector3& camPos, const GuideParams& params);

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	// ── 見た目の調整用 ──
	float m_TrackWidthRatio = 0.8f;		// トラックの幅（隣のレーンとの間隔に対する割合）
	float m_TrackBehind = 0.06f;		// トラックを判定円より手前にどれだけはみ出させるか（トラック全長に対する割合）
	float m_TrackIntensity = 0.35f;		// トラックの明るさ

	float m_StrikeCoreWidth = 5.0f;		// ストライクラインの芯の太さ
	float m_StrikeGlowWidth = 28.0f;	// ストライクラインのぼかしの太さ
	float m_StrikeIntensity = 0.8f;		// ストライクラインの明るさ
	float m_StrikeBeatPulse = 0.5f;		// 拍の頭でストライクラインが明るくなる量

	float m_PadSize = 85.0f;			// 判定円のパッドの大きさ
	float m_PadIntensity = 0.6f;		// パッドの普段の明るさ
	float m_PadPressIntensity = 1.4f;	// 押しているときのパッドの明るさ

	float m_BeatLineWidth = 4.0f;		// 拍の線の太さ
	float m_BarLineWidth = 9.0f;		// 小節線の太さ
	float m_BeatLineIntensity = 0.45f;	// 拍の線の明るさ
	float m_BarLineIntensity = 0.9f;	// 小節線の明るさ
};
