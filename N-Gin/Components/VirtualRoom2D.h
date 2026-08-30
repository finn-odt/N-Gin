#pragma once

#include <DirectXMath.h>

struct VirtualRoom2D
{
    DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f  };
    float width = 150.0f;
    float height = 150.0f;
    // only with wrapping, not repulsion
};