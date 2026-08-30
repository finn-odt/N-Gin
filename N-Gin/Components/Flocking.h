#pragma once

#include <DirectXMath.h>

struct Flocking
{
    float maxSpeed = 8.0f;
    float maxForce = 2.0f;

    float awarenessRadius = 12.0f;

    float separationRadius = 5.0f;
    float separationWeight = 1.5f;

    float alignmentWeight = 1.0f;

    float cohesionWeight = 0.4f;
};