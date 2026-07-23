#pragma once
#include "Object.h"
#include "StaticMesh.h"
#include "Texture2D.h"
#include "MeshRenderer.h"
#include "Material.h"

class SkyBox : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

private:
	float m_Size = 1500.0f;

	MeshRenderer m_MeshRenderer;

	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<std::unique_ptr<Material>> m_Materials;

	Texture2D m_SkyBoxIndex[6];
};

