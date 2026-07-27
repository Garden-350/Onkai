#include "Particle.h"
#include <DirectXMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
    float Lerp(float s, float e, float t) { return s + t * (e - s); }
}

Particle::Particle()
{
}

Particle::Particle(const DirectX::SimpleMath::Vector3& pos, float lifeTime, float gravity, const DirectX::SimpleMath::Color& colorStart, const DirectX::SimpleMath::Color& colorEnd, float colorChangeStartRate, float sizeStart, float sizeEnd, const DirectX::SimpleMath::Vector3& velocity, float deg)
{
    InitParam(pos, lifeTime, gravity, colorStart, colorEnd, colorChangeStartRate,
        sizeStart, sizeEnd, velocity, deg);
}

void Particle::InitParam(const DirectX::SimpleMath::Vector3& pos, float lifeTime, float gravity, const DirectX::SimpleMath::Color& colorStart, const DirectX::SimpleMath::Color& colorEnd, float colorChangeStartRate, float sizeStart, float sizeEnd, const DirectX::SimpleMath::Vector3& velocity, float deg)
{
    m_basePos = pos;
    m_pos = Vector3::Zero;
    m_lifeTime = lifeTime;
    m_gravity = gravity;
    m_colorStart = colorStart;
    m_colorEnd = colorEnd;
    m_color = colorStart;
    m_colorChangeStartRate = colorChangeStartRate;
    m_sizeStart = sizeStart;
    m_sizeEnd = sizeEnd;
    m_scale = sizeStart;
    m_velocity = velocity;
    m_deg = deg;
    m_time = 0.0f;
    m_mode = MoveMode::Oblique;
}

void Particle::Update(float tick)
{
    if (m_mode == MoveMode::Stop) return;

    m_time += tick;
    if (m_time >= m_lifeTime)
    {
        m_mode = MoveMode::Stop;
        return;
    }

    float t = m_time / m_lifeTime;
    m_scale = Lerp(m_sizeStart, m_sizeEnd, t);

    // 斜方投射：初速 + 重力
    float rad = m_deg * XM_PI / 180.0f;
    m_pos.x = m_velocity.x * m_time * cosf(rad);
    m_pos.y = m_velocity.y * m_time * sinf(rad) - m_gravity * m_time * m_time * 0.5f;
    m_pos.z = m_velocity.z * m_time * cosf(rad);

    ColorUpdate();
}

void Particle::ColorUpdate()
{
    float borderTime = m_lifeTime * m_colorChangeStartRate;
    if (borderTime > m_time)
    {
        m_color = m_colorStart;
        return;
    }

    float sec1 = m_lifeTime - borderTime;
    float sec2 = m_time - borderTime;
    float t = (sec1 > 0.0001f) ? sec2 / sec1 : 1.0f;
    if (t > 1.0f) t = 1.0f;

    m_color = Color(
        Lerp(m_colorStart.x, m_colorEnd.x, t),
        Lerp(m_colorStart.y, m_colorEnd.y, t),
        Lerp(m_colorStart.z, m_colorEnd.z, t),
        Lerp(m_colorStart.w, m_colorEnd.w, t)
    );
}
