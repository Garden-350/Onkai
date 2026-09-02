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
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class Player : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	int GetSide() const { return m_Side; }	// -1:左(J/K/L) 0:中央 1:右(S/D/F)
	DirectX::SimpleMath::Vector3 GetPosition() const { return m_Position; }

	// 立ち位置の基準（左右移動の中心）とレーン幅の調整用
	void SetBasePosition(const DirectX::SimpleMath::Vector3& pos) { m_BasePosition = pos; m_Position = pos; m_VisualX = pos.x; }
	void SetMoveRange(float range) { m_MoveRange = range; }

private:
	int   m_Side = 0;
	float m_VisualX = 0.0f;
	float m_MoveRange = 150.0f;
	float m_MoveSpeed = 800.0f;
	// 判定レーン(y=-55, z=-210)より手前・やや下に配置
// 判定レーン(y=-55, z=-210)より手前・低い位置＝カメラに一番近い場所
	DirectX::SimpleMath::Vector3 m_BasePosition = DirectX::SimpleMath::Vector3(0.0f, -60.0f, -380.0f);
	DirectX::SimpleMath::Vector3 m_Position = DirectX::SimpleMath::Vector3(0.0f, -60.0f, -380.0f);
	DirectX::SimpleMath::Vector3 m_Rotation = DirectX::SimpleMath::Vector3(1.57f, 0.0f, 0.0f);

	// スケールは必ずVector3で持つ（float型だと等倍拡大しかできず桁を間違えやすい）
	DirectX::SimpleMath::Vector3 m_Size = DirectX::SimpleMath::Vector3(0.25f, 0.25f, 0.25f);

	Shader m_Shader; // 大文字Sに統一
	MeshRenderer m_MeshRenderer;
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<std::unique_ptr<Material>> m_Materials;

	bool m_Started = false;
	std::chrono::steady_clock::time_point m_LastUpdateClock;
};