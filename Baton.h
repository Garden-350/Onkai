#pragma once
#include "Object.h"
#include "MeshRenderer.h"
#include "StaticMesh.h"
#include "utility.h"
#include "Material.h"
#include "Collision.h"

class Baton : public Object
{
private:

	// 描画の為の情報（メッシュに関わる情報）
	MeshRenderer m_MeshRenderer; // 頂点バッファ・インデックスバッファ・インデックス数

	//速度
	DirectX::SimpleMath::Vector3 m_Velocity =
		DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);

	//加速度
	DirectX::SimpleMath::Vector3 m_Acceleration =
		DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	// 描画の為の情報（見た目に関わる部分）
	std::vector<std::unique_ptr<Material>> m_Materials;
	std::vector<SUBSET> m_subsets;
	std::vector<std::unique_ptr<Texture>> m_Textures; // テクスチャ

	float moveSpeed = 1.0f;
	float roteSpeed = 0.05f;


public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;
};

