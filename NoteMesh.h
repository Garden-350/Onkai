#pragma once
#include "ModelUtil.h"
#include <SimpleMath.h>

//新しいノーツが自分のモデルを持つ場合はこれを一つ持っておく
class NoteMesh
{
public:
	bool Load(const char* modelPath, const char* texDir);
	bool IsLoaded() const { return m_Loaded; }

	//worldの位置、大きさ、向きでcolorの色を付けて描く
	void Draw(const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Vector3& color);

	bool SetTexture(const char* texturePath) { return ModelUtil::ApplyTexture(m_Materials, m_Textures, texturePath); }
private:
	bool m_Loaded = false;
	MeshRenderer m_Renderer;
	Shader m_Shader;
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Material>> m_Materials;
	std::vector<std::unique_ptr<Texture>> m_Textures;
};

