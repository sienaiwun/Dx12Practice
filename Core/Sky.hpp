#pragma once
#include "pch.h"

class ManagedTexture;
class Sky
{
public:
    Sky() :m_texture(),
        m_scale_z(1.5f) {};

    Sky(const Sky& sky) = default;

    Sky(Sky&& sky) noexcept = default;

    ~Sky() = default;

    Sky& operator=(const Sky& sky) noexcept = default;

    Sky& operator=(Sky&& sky) noexcept = default;

    [[nodiscard]]
    inline float GetScaleZ() const noexcept {
        return m_scale_z;
    }

    void SetTexture(std::shared_ptr<const ManagedTexture>  texture) {
        m_texture = std::move(texture);
    }

    [[nodiscard]]
    std::shared_ptr<const ManagedTexture> GetTexture() const noexcept
    {
        return m_texture;
    }

private:
    float m_scale_z;
    std::shared_ptr<const ManagedTexture> m_texture;
};