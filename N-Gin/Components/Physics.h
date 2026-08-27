#pragma once

#include <DirectXMath.h>

struct Physics
{
    DirectX::XMFLOAT3 linearVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 angularVelocity{ 0.0f, 0.0f, 0.0f };
    bool useGravity = true;
};