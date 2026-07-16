#pragma once
#include "Object.h"
#include "Texture.h"
#include "MeshRenderer.h"
#include "StaticMesh.h"
#include "utility.h"
#include "Material.h"
#include "Collision.h"

class DebugCollision
{
	// 頂点データ
	std::vector<VERTEX_3D> m_Vertices;

	//インデックスデータ
	std::vector<unsigned int> m_Indices;

	// 描画の為の情報（メッシュに関わる情報）
	IndexBuffer	 m_IndexBuffer; // インデックスバッファ
	VertexBuffer<VERTEX_3D>	m_VertexBuffer; // 頂点バッファ

	// 描画の為の情報（見た目に関わる部分）
	Shader m_Shader; // シェーダー
	Texture m_texture;	//テクスチャ
private:
	//何もなし

public:
	void Init();
	void Draw(const Collision::AABB& aabb, Camera* cam);
	void Uninit();
};

