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
#include <functional>

class Enemy : public Object
{
public:
	using EventFn = std::function<void()>;

	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;
	void SetOnDying(EventFn fn) { m_OnDying = fn; }			//瀕死
	void SetOnDefeated(EventFn fn) { m_OnDefeated = fn; }	//とどめ
	void SetOnExploded(EventFn fn) { m_OnExploded = fn; }	//爆発
	void Damage(float amount)
	{
		if (m_State == State::Defeated) return; //倒された後の攻撃は無効
		m_Hp -= amount;
		if (m_Hp < kDyingHp) m_Hp = kDyingHp;	//とどめの前は瀕死
		m_HitTimer = 0.25f;
		if (m_State == State::Alive && m_Hp <= kDyingHp)
		{
			m_State = State::Dying;	//瀕死
			if (m_OnDying) m_OnDying;
		}
	}
	void Finish();	//とどめ　最後のノーツがこいつを呼ぶ

	bool IsDying() const { return m_State == State::Dying; }
	bool IsDefeated() const { return m_State == State::Defeated; }

	//色を変える
	//color : 変える色　strength : 0～1（1 でその色一色）　duration : 元に戻るまでの秒数
	void Flash(const DirectX::SimpleMath::Vector3& color, float strength, float duration);

	float GetHpRatio() const { return m_Hp / m_MaxHp; }

	DirectX::SimpleMath::Vector3 GetPosition() const { return m_Position; }

	static constexpr float kDyingHp = 1.0f;	//瀕死ライン　最大値からの1%
	static constexpr float kExplodeTime = 0.9f;	//とどめから爆発までの秒数
private:
	enum class State
	{
		Alive,	//生きてる
		Dying,	//瀕死
		Defeated,	//死
	};
	State m_State = State::Alive;

	float m_DefeatTime = 0.0f;	//とどめをさしてからの秒数
	float m_Size = 200.0f;
	float m_MaxHp = 100.0f;
	float m_Hp = 100.0f;
	float m_HitTimer = 0.0f;	//攻撃を受けてから揺れの残り時間
	float m_DyingBlink = 0.0f;	//瀕死の時の点滅の残り時間

	float CurrentFlash() const;	//今の光り具合（0～1）

	bool m_Exploded = false;	//爆発？

	EventFn m_OnDying, m_OnDefeated, m_OnExploded;

	struct FlashParams { DirectX::SimpleMath::Vector4 color; };	//シェーダーへ送る形（16バイト）
	DirectX::SimpleMath::Vector3 m_FlashColor = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
	float m_FlashStrength = 0.0f;	//光った瞬間の強さ
	float m_FlashTime = 0.0f;		//元に戻るまでの残り時間
	float m_FlashDuration = 0.1f;	//元に戻るまでの全体の時間

	DirectX::SimpleMath::Vector3 m_Position = DirectX::SimpleMath::Vector3(0.0f, 30.0f, 200.0f);
	DirectX::SimpleMath::Vector3 m_Rotation = DirectX::SimpleMath::Vector3(1.57f, 3.14f, 0.0f);
	Shader m_Shader; // m_shaderのままだとPlayer同様タイポの温床になるので大文字に統一推奨
	MeshRenderer m_MeshRenderer;
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<std::unique_ptr<Material>> m_Materials;
};

