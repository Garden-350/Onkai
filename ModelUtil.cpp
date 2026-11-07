#include "ModelUtil.h"
#include "Renderer.h"
#include "Shader.h"
#include <filesystem>

using namespace DirectX::SimpleMath;

namespace ModelUtil
{
	bool LoadOptionalModel(
		const char* modelPath, const char* texDir,
		MeshRenderer& renderer, Shader& shader,
		std::vector<SUBSET>& subsets,
		std::vector<std::unique_ptr<Material>>& materials,
		std::vector<std::unique_ptr<Texture>>& textures)
	{
		//ファイルがなければ読む前にfalseを返す
		if (!std::filesystem::exists(modelPath))
		{
			OutputDebugStringA("[LoadOptionalModel] Model file does not exist\n");
			return false;
		}

		StaticMesh mesh;
		mesh.Load(modelPath, texDir);

		// サブセットが空 = 実質ロード失敗とみなす
		if (mesh.GetSubsets().empty())
		{
			OutputDebugStringA("[LoadOptionalModel] Failed to load mesh\n");
			return false;
		}

		renderer.Init(mesh);
		shader.Create("shader/litTextureVS.hlsl", "shader/litTexturePS.hlsl");
		subsets = mesh.GetSubsets();
		textures = mesh.GetTextures();

		std::vector<MATERIAL> mats = mesh.GetMaterials();
		for (auto& m : mats)
		{
			auto mat = std::make_unique<Material>();
			mat->Create(m);
			materials.push_back(std::move(mat));
		}
		return true;
	}

	bool ApplyTexture(std::vector<std::unique_ptr<Material>>& materials, std::vector<std::unique_ptr<Texture>>& textures, const char* texturePath)
	{
		if (!std::filesystem::exists(texturePath))
		{
			OutputDebugStringA("[ApplyTexture] Texture file does not exist\n");
			return false;
		}
		textures.resize(materials.size());	//マテリアルの数だけテクスチャを確保
		for (size_t i = 0; i < materials.size(); i++)
		{
			auto tex = std::make_unique<Texture>();
			if (!tex->Load(texturePath)) return false;	//同じ画像は使いまわし
			textures[i] = std::move(tex);
			materials[i]->SetTextureEnable(true);
			materials[i]->Update();
		}

		return true;
	}

	void DrawModel(MeshRenderer& renderer, Shader& shader,
		std::vector<SUBSET>& subsets,
		std::vector<std::unique_ptr<Material>>& materials,
		std::vector<std::unique_ptr<Texture>>& textures,
		const Matrix& worldConst)
	{
		Matrix world = worldConst; // const→非constコピー
		Renderer::SetWorldMatrix(&world);
		shader.SetGPU();
		renderer.BeforeDraw();

		for (size_t i = 0; i < subsets.size(); i++)
		{
			materials[subsets[i].MaterialIdx]->SetGPU();
			if (materials[subsets[i].MaterialIdx]->isTextureEnable())
				textures[subsets[i].MaterialIdx]->SetGPU();

			renderer.DrawSubset(
				subsets[i].IndexNum,
				subsets[i].IndexBase,
				subsets[i].VertexBase
			);
		}
	}
	void DrawModelWithOverrideTexture(MeshRenderer& renderer, Shader& shader, std::vector<SUBSET>& subsets, std::vector<std::unique_ptr<Material>>& materials, Texture& overrideTexture, const DirectX::SimpleMath::Matrix& worldConst)
	{
		Matrix world = worldConst;
		Renderer::SetWorldMatrix(&world);
		shader.SetGPU();
		renderer.BeforeDraw();

		for (size_t i = 0; i < subsets.size(); i++)
		{
			materials[subsets[i].MaterialIdx]->SetGPU();
			overrideTexture.SetGPU();

			renderer.DrawSubset
			(
				subsets[i].IndexNum,
				subsets[i].IndexBase,
				subsets[i].VertexBase
			);
		}
	}
}
