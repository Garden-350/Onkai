#pragma once
#include "VertexBuffer.h"
#include "shader.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include <SimpleMath.h>

//判定時刻に1つの円になる。そうそれは円環の交わりが如く
class ClosingRingGuide
{
public:
	void Init();
	// center   : リングの中心（ノーツの現在位置をそのまま渡す）
	// scale    : リングの直径
	// progress : 0〜1。1でちょうど円が閉じる（＝判定時刻ちょうど）
	// color    : ノーツ(レーン)の色
	void Draw(const DirectX::SimpleMath::Vector3& center, float scale,
		float progress, const DirectX::SimpleMath::Vector3& color);

private:
	struct ClosingRingParms
	{
		float progress;
		float padding0[3];	// 16byteアラインメント
		DirectX::SimpleMath::Vector4 color;
	};//sizeof == 32

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
};

