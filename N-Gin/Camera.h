#pragma once

#include <DirectXMath.h>

enum class ProjectionType
{
    Perspective,
    Orthographic
};

struct Camera
{
    ProjectionType projectionType = ProjectionType::Perspective;

    // simple camera with a position, a target (for the look-direction),
    // a Field Of View, a Near and a Far Plane
    DirectX::XMFLOAT3 position{ 2.0f, 2.0f, -5.0f };
    DirectX::XMFLOAT3 target{ 0.0f, 0.0f, 0.0f };

    float fov = DirectX::XM_PIDIV4;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    float orthographicSize = 10.0f;
};