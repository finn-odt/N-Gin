#pragma once

#include <DirectXMath.h>

enum class LightType : uint32_t
{
    Directional,
    Point,
    Spot
};

struct Light
{
    LightType type = LightType::Directional;

    bool isStatic = true;

    DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };

    float intensity = 1.0f;

    // direction/rotation comes from Transform
    // position comes from Transform
    float range = 10.0f;

    float innerAngle = 30.0f;;  // inner Cone - theta (full light)
    float outerAngle = 45.0f;;  // outer Cone - phi (fall off)

    // for the future:
    bool castShadows = false;
};

struct GPULight
{
    DirectX::XMFLOAT3 position;
    float range;

    DirectX::XMFLOAT3 direction;
    float intensity;

    DirectX::XMFLOAT3 color;
    uint32_t type;

    float innerConeCos;
    float outerConeCos;
    float padding[2];
};
// aligned with 16-byte packages -> GPU friendly