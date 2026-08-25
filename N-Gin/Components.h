#pragma once

#include "Usings.h"

#include <DirectXMath.h>

// Components (as raw data containers)

// use GetComponentTypeId<Position>() as ID

struct Transform
{
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};

struct Physics
{
    DirectX::XMFLOAT3 linearVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 angularVelocity{ 0.0f, 0.0f, 0.0f };
    bool useGravity = true;
};

using MeshHandle = uint32_t;
using MaterialHandle = uint32_t;

constexpr MeshHandle INVALID_MESH = 0;
constexpr MaterialHandle INVALID_MATERIAL = 0;

struct MeshRenderer
{
    MeshHandle mesh = INVALID_MESH;
    MaterialHandle material = INVALID_MATERIAL;
    bool visible = true;
};