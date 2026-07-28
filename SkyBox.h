#pragma once
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"
#include "StaticMesh.h"
#include "Texture2D.h"
#include "MeshRenderer.h"
#include "Material.h"
#include <array>
#include <vector>
class SkyBox : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

private:
	// Camera の farPlane = 1000 より小さくする
	static constexpr float m_Size = 900.0f;

	MeshRenderer m_MeshRenderer;

	std::vector<VERTEX_3D> m_Vertices;
	std::vector<unsigned int> m_Indices;
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Texture>> m_Texture;
	std::vector<std::unique_ptr<Material>> m_Materials;

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	std::array<Texture, 6> m_Textures;
	std::array<bool, 6> m_TextureLoaded{};

	Texture2D m_SkyBoxIndex[6];

	//スカイボックスの内側を描画するためのラスタライザ設定
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_InsideRasterizer;
};

