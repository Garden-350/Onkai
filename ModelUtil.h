#pragma once
#include "StaticMesh.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Texture.h"
#include "Shader.h"
#include <SimpleMath.h>
#include <vector>
#include <memory>

// ============================================================
// ModelUtil : ChartObject.cpp内にあった「無くても構わない補助モデルの
//   読み込み」と「モデル一式の描画」の共通処理を、他クラス(HomingBeamSystemなど)
// ============================================================
namespace ModelUtil
{
	// LoadOptionalModel : 「無くても構わない」補助モデルの読み込み共通処理。
	//   ロード失敗してもfalseを返すだけで、呼び出し側はTAPモデル等に
	//   フォールバックできるようにするためのヘルパー。
	//
	//   注意：StaticMesh::Load は戻り値がvoidの実装のため、
	//         ロード成功可否は「サブセットが1つ以上あるか」で簡易判定している。
	bool LoadOptionalModel(
		const char* modelPath, const char* texDir,
		MeshRenderer& renderer, Shader& shader,
		std::vector<SUBSET>& subsets,
		std::vector<std::unique_ptr<Material>>& materials,
		std::vector<std::unique_ptr<Texture>>& textures);

	// DrawModel : 指定したモデル一式(subsets/materials/textures)を
	//             指定ワールド行列で1回描画する共通処理。
	//
	//   Renderer::SetWorldMatrix が非constの Matrix* を要求するため、
	//   const参照で受け取った行列を一度ローカル変数へコピーしてから渡している。
	void DrawModel(MeshRenderer& renderer, Shader& shader,
		std::vector<SUBSET>& subsets,
		std::vector<std::unique_ptr<Material>>& materials,
		std::vector<std::unique_ptr<Texture>>& textures,
		const DirectX::SimpleMath::Matrix& worldConst);
}