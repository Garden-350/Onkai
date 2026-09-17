#pragma once
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Texture.h"
#include "Renderer.h"
#include <SimpleMath.h>

class MagicCircleQuad
{
private:
	struct MagicCircleParms
	{
		float progress;
		float padding0[3];	//16バイトアライメント
		DirectX::SimpleMath::Vector4 glowColor;
		DirectX::SimpleMath::Vector4 tintColor;
	};	//sizeof == 48

	Shader m_Shader;
	Texture m_Texture;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

public:
public:
	// psPathを省略した場合は今まで通りノーツ出現演出と同じシェーダーを使う
	void Init(const char* texturePath, const char* psPath = "shader/magicCireclePS.hlsl");

	// center   : ワールド座標（クワッドの中心）
	// scale    : クワッドの一辺の大きさ
	// progress : 描き終わり具合(0〜1)。1で完成
	// glowColor: 描画中の縁を光らせる色
	void Draw(const DirectX::SimpleMath::Vector3& center, float scale,
		float progress, const DirectX::SimpleMath::Vector3& glowColor,
		const DirectX::SimpleMath::Vector3& tintColor);
};

