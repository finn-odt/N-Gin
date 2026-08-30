#pragma once

#include <DirectXMath.h>

struct Flocking
{
    float maxSpeed = 8.0f;
    float maxForce = 2.0f;

    float turnSpeed = 4.0f;   // max radians / second (4 rad/s is roughly 229°/s)
    float turnResponse = 4.0f;   // how aggressively it corrects angle

    float awarenessRadius = 12.0f;

    float separationRadius = 5.0f;
    float separationWeight = 1.5f;

    float alignmentWeight = 1.0f;

    float cohesionWeight = 0.4f;
};