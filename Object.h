#pragma once
#include "Camera.h"
#include "Shader.h"

class Object {
protected:
	// SRT情報（姿勢情報）
	DirectX::SimpleMath::Vector3 m_Position = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3 m_Rotation = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	DirectX::SimpleMath::Vector3 m_Scale = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
	DirectX::SimpleMath::Vector3 m_Color = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
	// 描画の為の情報（見た目に関わる部分）
	Shader m_Shader; // シェーダー

	//そのオブジェクトを消すかどうかのbool
	bool m_IsAlive = true;

public:
	virtual ~Object() {}
	//仮想デストラクタ（※派生クラスのリソース解放のために必要）

	virtual void Init()=0;
	virtual void Update() = 0;
	virtual void Draw(Camera* cam) = 0;
	virtual void Uninit() = 0;
	virtual Camera::Layer GetLayer() const { return Camera::Layer::World; }

	void Kill() { m_IsAlive = false; }
	bool IsAlive() const { return m_IsAlive; }

	// 位置の取得
	DirectX::SimpleMath::Vector3 GetPosition() const { return m_Position; }
};
