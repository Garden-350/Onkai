#pragma once
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

	//毎フレームChartObjectから渡してもらう情報
	struct Frame
	{
		DirectX::SimpleMath::Vector3 judgePos[kLaneCount];	//今の判定円の位置
		DirectX::SimpleMath::Vector3 trackOffset[kLaneCount];//判定円ー＞トラックの奥の端までのベクトル（ハイスピ込み）
		DirectX::SimpleMath::Vector3 laneColor[kLaneCount];
		float laneGlow[kLaneCount];	//0~1 キーを押している/判定した直後ほど大きい
		float musicTime;
		float bpm;
		int beatsPerBar;	//何拍ごとに太い線にするか
		float approachSeconds;	//ノーツがトラックの奥から判定円まで来るのにかかる秒数
		DirectX::SimpleMath::Vector3 lateral[kLaneCount];	//判定円が並ぶ向き（隣とつながらない時に使う）
		int group[kLaneCount];	//同じ数字で隣り合うレーンだけ線でつなぐ（向きの番号）
	};
	void Init();
	void Draw(Camera* cam, const Frame& f);

private:
	struct GuideParams
	{
		DirectX::SimpleMath::Vector4 color;
		float mode;		// 0=ライン, 1=トラック, 2=パッド
		float param1;	// mode0:両端のフェード幅 / mode2:押し込み具合
		float param2;
		float padding;
	};	//sizeof == 32

	//centerを中心に横幅分・縦の長さ分の板を描く。uv.y=0が+axisV側
	void DrawBasis(const DirectX::SimpleMath::Vector3& center,
		const DirectX::SimpleMath::Vector3& axisU, const DirectX::SimpleMath::Vector3& axisV,
		const GuideParams& params);
	//from->toを結ぶ、カメラの方を向いた帯
	void DrawRibbon(const DirectX::SimpleMath::Vector3& from, const DirectX::SimpleMath::Vector3& to,
		float width, const DirectX::SimpleMath::Vector3& camPos, const GuideParams& params);

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	//見た目調整用
	float m_TrackWidthRatio = 0.8f;		//トラックの幅（隣のレーンとの間隔に対する割合）
	float m_TrackBehind = 0.06f;		//トラックを判定円より手前にどれだけはみ出させるか（トラック全長に対する割合）
	float m_TrackIntensity = 0.35f;		//トラックの明るさ

	float m_StrikeCoreWidth = 5.0f;		//ストライクラインの芯の太さ
	float m_StrikeGlowWidth = 28.0f;	//ストライクラインのぼかしの太さ
	float m_StrikeIntensity = 0.8f;		//ストライクラインの明るさ
	float m_StrikeBeatPulse = 0.5f;		//拍の頭でストライクラインが明るくなる量

	float m_PadSize = 85.0f;			//判定円のバッドの大きさ
	float m_PadIntensity = 0.6f;		//バッドの普段の明るさ
	float m_PadPressIntensity = 1.4f;	//押しているときのバッドの明るさ

	float m_BeatLineWidth = 4.0f;		//拍の線の太さ
	float m_BarLineWidth = 9.0f;		//小節線の太さ
	float m_BeatLineIntensity = 0.45f;	//拍の線の明るさ
	float m_BarLineIntensity = 0.9f;	//小節戦の明るさ
};

