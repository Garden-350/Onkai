#pragma once
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Texture.h"
#include "Renderer.h"
#include <SimpleMath.h>

class BarrierPlate
{
private:
	struct BarrierPlateParms
	{
		float alpha;
		float padding0[3];	//16byteアライメント
		DirectX::SimpleMath::Vector4 tintColor;
	};	//sizeof == 32
	Shader m_Shader;
	Texture m_Texture;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

public:
	void Init(const char* texrurePath);

	void Draw(const DirectX::SimpleMath::Vector3& center, float scale,
		float alpha, const DirectX::SimpleMath::Vector3& tintColor);
	// center    : ワールド座標（判定キーの位置）
	// scale     : 板の一辺の大きさ
	// alpha     : 板全体の不透明度(0〜1)。明滅などに使う
	// tintColor : レーン色（GetLaneColorの戻り値をそのまま渡す）
};

