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
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

VOut VS(float3 position : POSITION, float3 normal : NORMAL, float2 texCoord : TEXCOORD0)
{
    VOut output;

    float4 p = float4(position, 1.0f);  // 3D to 4D: for homogeneous matrix multiplication

    p = mul(p, world);  // local -> world space
    p = mul(p, view);  // world -> view/camera space
    p = mul(p, projection); // view space -> clip space

    output.position = p;

    // make the normal follow the object's rotation
    output.normal = normalize(
        mul(normal, (float3x3)world)
    );

    output.texCoord = texCoord;

    return output;
}

float4 PS(VOut input) : SV_TARGET
{
    float3 normal = normalize(input.normal);

    /*// Simple directional light
    float3 lightDirection = normalize(float3(5.0f, 5.0f, -15.0f));  // for debugging

    float diffuse = saturate(dot(normal, -lightDirection));

    float lighting = 0.2f + diffuse * 0.8f;  // 20% ambient light, and then 80% diffuse

    // Procedural checkerboard using UV coordinates
    float checker = fmod(floor(input.texCoord.x * 8.0f) + floor(input.texCoord.y * 8.0f), 2.0f);

    float3 colorA = float3(0.15f, 0.3f, 0.8f);
    float3 colorB = float3(0.8f, 0.9f, 1.0f);

    float3 baseColor = lerp(colorA, colorB, checker);

    return float4(baseColor * lighting, 1.0f);*/

    return float4(input.texCoord.xy, 1.0f, 1.0f);
}