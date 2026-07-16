#pragma once
#include "Object.h"
#include "MeshRenderer.h"
#include "StaticMesh.h"
#include "utility.h"
#include "Material.h"
#include "Collision.h"

using namespace DirectX::SimpleMath;
class Note : public Object
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
	std::vector<Note*> m_Notes;
	std::vector<std::unique_ptr<Texture>> m_Textures; // テクスチャ

	DirectX::SimpleMath::Vector3 m_Direction =
		DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);

	int m_MaxNote = 16;	//一小節にどれだけ生成できるか

	float m_Speed = 2.0f;
	float m_LifeTime = 3.0f;  // 3秒で消える
	float m_Timer = 0.0f;

public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	void SetData(const Vector3& pos, const Vector3& dir);
	void RequestSpawn(const Vector3& pos, const Vector3& dir);
};

