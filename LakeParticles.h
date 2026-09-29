#pragma once
//突風で舞う葉っぱと夜のホタルとか

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include "LakeParms.h"
#include <SimpleMath.h>
#include <vector>
#include <random>

class Camera;
class LakeParticles
{
public:
	void Init(float shoreBaseZ);
	//p:その時の時間帯・風(TitleLakeが作ったもの)
	void Update(float dt, const LakeParms& p);
	void Draw(Camera* cam, const LakeParms& p);

private:
	struct Leaf
	{
		DirectX::SimpleMath::Vector3 pos;
		DirectX::SimpleMath::Vector3 vel;
		float rot = 0.0f;		// 画面の中での回転
		float spin = 0.0f;		// 回転の速さ
		float size = 6.0f;
		float life = 0.0f;
		float phase = 0.0f;
		DirectX::SimpleMath::Vector3 albedo;
		bool alive = false;
	};
	struct Firefly
	{
		DirectX::SimpleMath::Vector3 anchor;	// このあたりを飛ぶ
		float phase = 0.0f;
		float speed = 1.0f;
		float blink = 1.0f;
	};
	struct ParticleCB
	{
		DirectX::SimpleMath::Vector4 color;	// rgb=色（明るさ込み）, a=不透明度
		DirectX::SimpleMath::Vector4 misc;	// x=種類(0=葉 1=ホタル), y=露出
	};

	void SpawnLeaf(const LakeParms& p);
	void DrawQuad(const DirectX::SimpleMath::Vector3& center, float size, float rot, float aspect,
		const DirectX::SimpleMath::Vector3& camRight, const DirectX::SimpleMath::Vector3& camUp,
		const ParticleCB& cb);

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	std::vector<Leaf> m_Leaves;
	std::vector<Firefly> m_Fireflies;
	std::mt19937 m_Rng{ 1234 };
	float m_SpawnTimer = 0.0f;
	float m_Time = 0.0f;
	float m_ShoreBaseZ = 300.0f;
};

