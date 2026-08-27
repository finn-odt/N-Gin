#pragma once

#include "../Usings.h"

struct MeshRenderer
{
    MeshHandle mesh = INVALID_MESH;
    MaterialHandle material = INVALID_MATERIAL;
    bool visible = true;
};