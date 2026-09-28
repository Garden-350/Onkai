#pragma once
// ============================================================
// LakeGrass : 手前の岸に生えている草と、水際の葦（あし）。
//   1本1本の葉を細い板で作り、まとめて1回で描く。
//   風でなびく動きは頂点シェーダー(lakeGrassVS.hlsl)でやるので、CPU側は最初に形を作るだけ。
//   TitleLake が持っていて、TitleLake::Draw の中から呼ばれる。
// ============================================================
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include <SimpleMath.h>

class Camera;

class LakeGrass
{
public:
	// shoreBaseZ : 手前の岸までの距離（TitleLake と同じ値）
	void Init(float shoreBaseZ);
	// lakeCB : 時間帯・風の情報が入った定数バッファ（TitleLake が作ったもの）
	void Draw(Camera* cam, ID3D11Buffer* lakeCB);

private:
	void AddBlade(std::vector<VERTEX_3D>& verts, std::vector<unsigned int>& idx,
		const DirectX::SimpleMath::Vector3& base, float yaw, float leanYaw, float lean,
		float width, float height, const DirectX::SimpleMath::Vector3& color, float rnd);

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	unsigned int m_IndexCount = 0;

	// ── 見た目の調整用 ──
	int   m_GrassCount = 20000;		// 草の本数
	int   m_ReedCount = 480;		// 葦の本数
	float m_NearZ = 30.0f;			// 草が生えている一番手前（カメラの足元）
};
