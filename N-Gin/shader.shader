/*
register() is HLSL and defines which constant-buffer slot is bound to that buffer
b = constant buffer
0 = slot index 0
USECASE:     devcon->VSSetConstantBuffers(0, 1, &matrixBuffer);

Constant Buffers
b0 → matrices
b1 → lighting
b2 → material
b3 → something else

b0, b1, ...   constant buffers

t0, t1, ...   textures / shader resource views

s0, s1, ...   samplers

u0, u1, ...   unordered access views
*/

cbuffer MatrixBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
};

struct VOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VOut VS(float3 position : POSITION, float4 color : COLOR)
{
    VOut output;

    float4 p = float4(position, 1.0f);

    p = mul(p, world);
    p = mul(p, view);
    p = mul(p, projection);

    output.position = p;
    output.color = color;

    return output;
}

float4 PS(VOut input) : SV_TARGET
{
    return input.color;
}