#pragma once
#include "Object.h"
#include "StaticMesh.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Texture.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>

class Enemy : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	DirectX::SimpleMath::Vector3 GetPosition() const { return m_Position; }
private:
	float m_Size = 200.0f;

	DirectX::SimpleMath::Vector3 m_Position = DirectX::SimpleMath::Vector3(0.0f, 100.0f, 200.0f);
	DirectX::SimpleMath::Vector3 m_Rotation = DirectX::SimpleMath::Vector3(1.57f, 3.14f, 0.0f);
	Shader m_Shader; // m_shaderのままだとPlayer同様タイポの温床になるので大文字に統一推奨
	MeshRenderer m_MeshRenderer;
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<std::unique_ptr<Material>> m_Materials;
};

