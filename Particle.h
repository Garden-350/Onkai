#ifndef __PARTICLE_H__
#define __PARTICLE_H__

#include "Texture.h"
#include <vector>

#pragma once
// 球クラス
class Particle
{
public:
	enum MoveMode {
		Stop,
		Oblique
	};

public:
	Particle();
	Particle(DirectX::XMFLOAT3 pos,
		float lifeTime,
		float gravity,
		DirectX::XMFLOAT4 color_s,
		DirectX::XMFLOAT4 color_e,
		float color_r,
		float size_start,
		float size_end,
		DirectX::XMFLOAT3 vec,
		float mass,
		float ref,
		float deg,
		Texture* tex);

	void InitParam(DirectX::XMFLOAT3 pos,
		float lifeTime,
		float gravity,
		DirectX::XMFLOAT4 color_s,
		DirectX::XMFLOAT4 color_e,
		float color_r,
		float size_start,
		float size_end,
		DirectX::XMFLOAT3 vec,
		float mass,
		float ref,
		float deg,
		Texture* tex);

	void Update(float tick);
	void ColorUpdate();
	void Drow();

	void Reset();
	bool IsStop();

	bool HitSphere(Particle b);
	float GetRadius();
	DirectX::XMFLOAT3 GetPosition();
	float GetMass();
	DirectX::XMFLOAT3 GetSpeed3d();
	float Lerp(float s, float e, float t);

	bool FindPair(int num);
	void SetTexture(Texture* tex);
public:
	std::vector<int> hitPair;		// 衝突処理重複回避のため処理ペアの添え字を格納

private:
	DirectX::XMFLOAT4X4 m_mat;		// ワールド行列
	DirectX::XMFLOAT4 m_color;		// 球の色
	DirectX::XMFLOAT3 m_scale;		// 球のサイズ
	DirectX::XMFLOAT3 m_basePos;	// 初期座標
	DirectX::XMFLOAT3 m_pos;		// 初期座標からの差分
	const float m_baseRadius = 0.5f;// 1倍の時の球の半径※固定値
	float m_gravity = 9.8f;			// 重力
	float m_time = 0.0f;			// 時間
	DirectX::XMFLOAT3 m_speed3d = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);			// 初速3D
	float m_acceleration = 0.0f;	// 加速度
	float m_mass = 1.0f;			// 質量
	float m_lifeTime = 5.0f;		// 生存時間
	float m_refRate = 1.0f;			// 反発係数
	MoveMode m_mode = MoveMode::Oblique;	// パーティクル状態遷移
	DirectX::XMFLOAT4 m_color_start = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);	// 開始色
	DirectX::XMFLOAT4 m_color_end = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);		// 終了色
	float m_deg = 60.0f;					// 投射角度
	float m_colorChangeStartRate = 0.8f;
	float m_size_start = 1.0f;
	float m_size_end = 1.0f;
	Texture* texture;			// パーティクル用テクスチャ
};

#endif // __PARTICLE_H__