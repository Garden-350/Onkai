#pragma once
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"
#include "Material.h"
#include <memory>

// 地面にランダムに立ち並ぶビル群（背景装飾用。当たり判定は持たない）
class CityBuildings : public Object
{
private:
	std::vector<VERTEX_3D> m_Vertices;
	std::vector<unsigned int> m_Indices;

	IndexBuffer m_IndexBuffer;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;

	Texture m_Texture;
	std::unique_ptr<Material> m_Material;

	//1棟ぶんの箱を頂点データに追加する
	void AddBuilding(float centerX, float centerZ, float width, float depth, float height,
		const DirectX::SimpleMath::Color& tint);

public:
	void Init() override;
	void Update() override {}
	void Draw(Camera* cam) override;
	void Uninit() override {}
};