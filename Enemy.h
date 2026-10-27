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
	void Damage(float amount)
	{
		m_Hp -= amount;
		if (m_Hp < 0.0f) m_Hp = 0.0f;	// 0より下にはしない
		m_HitTimer = 0.25f;
	}

	//色を変える
	//color : 変える色　strength : 0～1（1 でその色一色）　duration : 元に戻るまでの秒数
	void Flash(const DirectX::SimpleMath::Vector3& color, float strength, float duration);

	float GetHpRatio() const { return m_Hp / m_MaxHp; }
	bool IsDefeated() const { return m_Hp <= 0.0f; }

	DirectX::SimpleMath::Vector3 GetPosition() const { return m_Position; }
private:
	float m_Size = 200.0f;
	float m_MaxHp = 100.0f;
	float m_Hp = 100.0f;
	float m_HitTimer = 0.0f;	//攻撃を受けてから揺れの残り時間

	float CurrentFlash() const;	//今の光り具合（0～1）

	struct FlashParams { DirectX::SimpleMath::Vector4 color; };	//シェーダーへ送る形（16バイト）
	DirectX::SimpleMath::Vector3 m_FlashColor = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
	float m_FlashStrength = 0.0f;	//光った瞬間の強さ
	float m_FlashTime = 0.0f;		//元に戻るまでの残り時間
	float m_FlashDuration = 0.1f;	//元に戻るまでの全体の時間

	DirectX::SimpleMath::Vector3 m_Position = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 200.0f);
	DirectX::SimpleMath::Vector3 m_Rotation = DirectX::SimpleMath::Vector3(1.57f, 3.14f, 0.0f);
	Shader m_Shader; // m_shaderのままだとPlayer同様タイポの温床になるので大文字に統一推奨
	MeshRenderer m_MeshRenderer;
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<std::unique_ptr<Material>> m_Materials;
};

