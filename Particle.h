#pragma once
#include <SimpleMath.h>

class Particle
{
public:
	enum class MoveMode {
		Stop,
		Oblique,
	};
	Particle();
    Particle(const DirectX::SimpleMath::Vector3& pos,
        float lifeTime, float gravity,
        const DirectX::SimpleMath::Color& colorStart,
        const DirectX::SimpleMath::Color& colorEnd,
        float colorChangeStartRate,
        float sizeStart, float sizeEnd,
        const DirectX::SimpleMath::Vector3& velocity,
        float deg);

    void InitParam(const DirectX::SimpleMath::Vector3& pos,
        float lifeTime, float gravity,
        const DirectX::SimpleMath::Color& colorStart,
        const DirectX::SimpleMath::Color& colorEnd,
        float colorChangeStartRate,
        float sizeStart, float sizeEnd,
        const DirectX::SimpleMath::Vector3& velocity,
        float deg);

    void Update(float tick);
    bool IsStop() const { return m_mode == MoveMode::Stop; }
    DirectX::SimpleMath::Vector3 GetPosition() const { return m_basePos + m_pos; }
    float GetScale() const { return m_scale; }
    DirectX::SimpleMath::Color GetColor() const { return m_color; }

private:
    void ColorUpdate();

    DirectX::SimpleMath::Vector3 m_basePos{};
    DirectX::SimpleMath::Vector3 m_pos{};
    DirectX::SimpleMath::Vector3 m_velocity{};
    DirectX::SimpleMath::Color m_color{ 1,1,1,1 };
    DirectX::SimpleMath::Color m_colorStart{ 1,1,1,1 };
    DirectX::SimpleMath::Color m_colorEnd{ 1,1,1,1 };
    float m_colorChangeStartRate = 0.8f;
    float m_scale = 1.0f;
    float m_sizeStart = 1.0f;
    float m_sizeEnd = 1.0f;
    float m_gravity = 9.8f;
    float m_time = 0.0f;
    float m_lifeTime = 1.0f;
    float m_deg = 60.0f;
    MoveMode m_mode = MoveMode::Oblique;
};

