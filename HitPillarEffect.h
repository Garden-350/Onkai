#pragma once
// ============================================================
// HitPillarEffect : 判定したときに判定円から伸びる「光の柱」を描くクラス。
//   HomingBeamSystem のビームモデルを流用するのをやめ、
//   カメラの方を向く板1枚＋シェーダー(spotlightPS.hlsl)で描く。
//   根本(base)を渡した位置に必ず柱の根本が来る。
//   ※ブレンドステート(加算)の設定は呼び出し側で行うこと。
// ============================================================
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include <SimpleMath.h>

class Camera;

class HitPillarEffect
{
public:
	void Init();

	// base      : 柱の根本（判定円の位置）
	// dir       : 柱が伸びる向き（正規化済み）
	// length    : 柱の長さ
	// baseWidth : 根本の太さ / topWidth : 先端の太さ
	// color     : 色 / intensity : 明るさ(0で見えない)
	// lampSize  : 根本の丸い光の大きさ(0で描かない)
	void Draw(Camera* cam,
		const DirectX::SimpleMath::Vector3& base, const DirectX::SimpleMath::Vector3& dir,
		float length, float baseWidth, float topWidth,
		const DirectX::SimpleMath::Vector3& color, float intensity, float lampSize);

private:
	struct PillarParams
	{
		DirectX::SimpleMath::Vector4 color;	// rgb=色, a=明るさ
		float mode;				// 0=光の柱, 1=根本のランプ（spotlightPS.hlslと同じ並び）
		float baseWidthRatio;	// 根本の幅 / 先端の幅
		float padding[2];
	};	// sizeof == 32

	void DrawQuad(const DirectX::SimpleMath::Matrix& world, const PillarParams& params);

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
};
