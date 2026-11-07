#include "NoteMesh.h"
#include "Renderer.h"

using namespace DirectX::SimpleMath;

bool NoteMesh::Load(const char* modelPath, const char* texDir)
{
	m_Loaded = ModelUtil::LoadOptionalModel(modelPath, texDir, m_Renderer, m_Shader, m_Subsets, m_Materials, m_Textures);
    return m_Loaded;
}

void NoteMesh::Draw(const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Vector3& color)
{
	if (!m_Loaded) return;

	Matrix worldMatrix = world;	//SetWorldMatrixはconstでない行列を受け取るためコピーする

	Renderer::SetWorldMatrix(&worldMatrix);
	m_Shader.SetGPU();
	m_Renderer.BeforeDraw();	//このモデルは頂点をGPUに渡す必要があるので、DrawSubsetの前に呼ぶ

	//タップと同じ：マテリアルの色をレーンの色にして光らせる
	for (size_t i = 0; i < m_Subsets.size(); i++)
	{
		Material* mat = m_Materials[m_Subsets[i].MaterialIdx].get();
		mat->SetDiffuse(DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f));
		mat->SetEmission(DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f));
		mat->Update();
		mat->SetGPU();
		if(mat->isTextureEnable())
			m_Textures[m_Subsets[i].MaterialIdx]->SetGPU();

		m_Renderer.DrawSubset(m_Subsets[i].IndexNum, m_Subsets[i].IndexBase, m_Subsets[i].VertexBase);
	}
}
