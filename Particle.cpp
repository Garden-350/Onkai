
#include <DirectXMath.h>

#include "Particle.h"
#include "math.h"
#include "Geometory.h"
#include "Billboard.h"
#include "DebugLog.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

Particle::Particle()
{
	m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_basePos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
	DirectX::XMStoreFloat4x4(&m_mat, DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z)));
}

Particle::Particle(DirectX::XMFLOAT3 pos, 
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
					Texture* tex)
{
	m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_basePos = pos;
	m_lifeTime = lifeTime;
	m_gravity = gravity;
	m_color = color_s;
	m_color_start = color_s;
	m_color_end = color_e;
	m_colorChangeStartRate = color_r;
	m_speed3d = vec;
	m_mass = mass;
	m_refRate = ref;
	m_deg = deg;
	SetTexture(tex);
	m_scale = DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f);
	DirectX::XMStoreFloat4x4(&m_mat, DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z)));
	m_mat = m_mat * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
}

void Particle::InitParam(DirectX::XMFLOAT3 pos,
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
	Texture* tex)
{
	m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_basePos = pos;
	m_lifeTime = lifeTime;
	m_gravity = gravity;
	m_color = color_s;
	m_color_start = color_s;
	m_color_end = color_e;
	m_colorChangeStartRate = color_r;
	m_speed3d = vec;
	m_mass = mass;
	m_refRate = ref;
	m_deg = deg;
	SetTexture(tex);
	m_scale = DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f);
	DirectX::XMStoreFloat4x4(&m_mat, DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z)));
	m_mat = m_mat * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
	m_time = 0.0f;
	m_mode = MoveMode::Oblique;
}

void Particle::Update(float tick)
{
	m_time += tick;

	float t = m_time / m_lifeTime;
	if (t > 1.0f) t = 1.0f;

	float scale = Lerp(m_size_start, m_size_end, t);
	m_scale = DirectX::XMFLOAT3(scale, scale, scale);


	// �������Ԑ؂�`�F�b�N
	if (m_time >= m_lifeTime) m_mode = MoveMode::Stop;

	switch (m_mode)
	{
	case MoveMode::Stop:
		// ��~
		break;
	case MoveMode::Oblique:					// �Ε�����
		int deg = m_deg * XM_PI / 180.0f;	//���W�A���ɕϊ�
		m_pos.x = m_speed3d.x * m_time * cos(deg);
		m_pos.y = m_speed3d.y * m_time * sin(deg) - m_gravity * m_time * m_time / 2;
		m_pos.z = m_speed3d.z * m_time * cos(deg);

		ColorUpdate();
		break;
	}
}

void Particle::ColorUpdate()
{
	// �������Ԃ��w�芄���ȏ�o�߂��Ă邩�H
	float borderTime = m_lifeTime * m_colorChangeStartRate;
	if (borderTime > m_time) return;
	// �������ԏC������color_end�ɂȂ�悤�ɐ��`�⊮����
	float sec1 = m_lifeTime - borderTime;
	float sec2 = m_time - borderTime;
	float t = sec2 / sec1;
	// �F�̐��`�⊮
	float r = Lerp(m_color_start.x, m_color_end.x, t);
	float g = Lerp(m_color_start.y, m_color_end.y, t);
	float b = Lerp(m_color_start.z, m_color_end.z, t);
	float a = Lerp(m_color_start.w, m_color_end.w, t);
	m_color = DirectX::XMFLOAT4(r,g,b,a);
}

void Particle::Drow()
{
	if (m_mode == MoveMode::Stop) return;
	// 座標行列作成
	DirectX::XMFLOAT4X4 mat;
	DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixTranspose(
		DirectX::XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z) *	// スケール
		DirectX::XMMatrixTranslation(m_pos.x + m_basePos.x, m_pos.y + m_basePos.y, m_pos.z + m_basePos.z)	// 座標
	));

	Billboard::SetWorld(mat);
	Billboard::SetColor(m_color);
	Billboard::SetBlendAdd(true);
	Billboard::SetTexture(texture);
	Billboard::Draw();
}

void Particle::Reset()
{
	m_pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_time = 0.0f;
}

bool Particle::IsStop()
{
	return m_mode == MoveMode::Stop;
}
bool Particle::HitSphere(Particle b)
{
	DirectX::XMFLOAT3 pos1 = GetPosition();
	DirectX::XMFLOAT3 pos2 = b.GetPosition();

	float outp = (pos2.x - pos1.x) * (pos2.x - pos1.x) +
				 (pos2.y - pos1.y) * (pos2.y - pos1.y) +
				 (pos2.z - pos1.z) * (pos2.z - pos1.z);

	float r1 = GetRadius();
	float r2 = b.GetRadius();
	float outr = (r1 + r2) * (r1 + r2);
	return outp <= outr;
}

float Particle::GetRadius()
{
	float scale = (m_scale.x + m_scale.y + m_scale.z) / 3;
	return m_baseRadius * scale;
}

DirectX::XMFLOAT3 Particle::GetPosition()
{
	return m_pos + m_basePos;
}

float Particle::GetMass()
{
	return m_mass;
}

DirectX::XMFLOAT3 Particle::GetSpeed3d()
{
	return m_speed3d;
}

bool Particle::FindPair(int num)
{
	bool b = false;
	for (int i : hitPair)
	{
		if (i == num) b = true;
	}
	return b;
}

// ���`��Ԋ֐�
float Particle::Lerp(float s, float e, float t) {
	return s + t * (e - s);
}

void Particle::SetTexture(Texture* tex)
{
	texture = tex;
}