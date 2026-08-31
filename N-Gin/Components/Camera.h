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

    float fov = DirectX::XM_PIDIV4 * (50.0f / 45.0f);
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    float orthographicSize = 10.0f;
};