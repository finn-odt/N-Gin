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

#define MAX_LIGHTS 16

struct LightData
{
    float3 position;
    float range;

    float3 direction;
    float intensity;

    float3 color;
    /**
    * Directional = 0
    * Point       = 1
    * Spot        = 2
    */
    uint type;

    float innerConeCos;
    float outerConeCos;
    float2 padding;
};

cbuffer CameraMatrixBuffer : register(b0)
{
    matrix view;
    matrix projection;

    float3 cameraPosition;
    float cameraPadding;
};

cbuffer MaterialBuffer : register(b1)
{
    float4 baseColor;

    float4 albedoUVTransform;
    // xy = tiling
    // zw = offset
    
    //bool isMetallic;  // NEW
};

cbuffer LightBuffer : register(b2)
{
    LightData lights[MAX_LIGHTS];

    float3 ambientColor;
    float ambientIntensity;

    uint lightCount;
    float3 lightBufferPadding;
};

Texture2D albedoMap     : register(t0);
Texture2D normalMap     : register(t1);
Texture2D smoothnessMap : register(t2);
Texture2D heightMap     : register(t3);

SamplerState textureSampler : register(s0);

struct VIn {
    // input slot 0 (per vertex)
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv : TEXCOORD0;
    float4 tangent  : TANGENT0;
    
    // input slot 1 (per instance)
    float4 world0 : INSTANCEWORLD0;
    float4 world1 : INSTANCEWORLD1;
    float4 world2 : INSTANCEWORLD2;
    float4 world3 : INSTANCEWORLD3;
};

struct VOut
{
    float4 clipPosition : SV_POSITION;
    float3 worldPosition : POSITION0;
    //float3 localPosition : XXXXXXX;
    float3 worldNormal : NORMAL;
    float2 uv : uv0;
    float4 tangentWS : TANGENT0;
};

float2 TransformTex(float2 uv, float4 transform)
{
    return uv * transform.xy + transform.zw;
}

// Vertex Shader
VOut VS(VIn input)
{
    VOut output;

    // Reconstruct this entity's world matrix from the instance buffer.
    float4x4 world = float4x4(
        input.world0,
        input.world1,
        input.world2,
        input.world3
    );

    //output.localPosition = input.position;

    // 3D to 4D: for homogeneous matrix multiplication (input.position)

    // local -> world space
    float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.worldPosition = worldPosition.xyz;

    float4 clipPosition = mul(worldPosition, view);  // world -> view/camera space
    
    clipPosition = mul(clipPosition, projection);  // view space -> clip space
    output.clipPosition = clipPosition;

    // rotate normal into world space
    float3 normalWS = normalize( mul(input.normal, (float3x3)world) );
    output.worldNormal = normalWS;

    output.uv = TransformTex(input.uv, albedoUVTransform);  // add offset and tiling

    float3 tangentWS = normalize( mul(input.tangent.rgb, (float3x3)world) );
    // re-orthogonalize tangent against normal
    tangentWS = normalize( tangentWS - normalWS * dot(normalWS, tangentWS) );

    output.tangentWS = float4(tangentWS, input.tangent.w);  // save W-axis, as this is only sign

    return output;
}

float3 CalculateLight(float3 normalWS, float3 viewDirWS, float3 lightDirWS, LightData light)
{                
    float NdotL = saturate(dot(normalWS, lightDirWS)); // dot-product of light and normal

    // if(isMetallic) -> diffuse isShader always black
    float3 diffuse = light.color * light.intensity * NdotL;  // diffuse-color could be added

    float3 specular = float3(0.0f, 0.0f, 0.0f);

    if (NdotL > 0.0f)  // only if surface faces the light
    {
        float3 reflected = reflect(-lightDirWS, normalWS);  // get Light Reflection Vector according to face Normal

        const float shininess = 32.0f;
        float specularAmount = pow( saturate(dot(reflected, viewDirWS)), shininess );

        specular = light.color * light.intensity * specularAmount;  // specular-color could be added
    }

    return diffuse + specular;
}

float3 CalculateDirectionalLight(float3 normalWS, float3 viewDirWS, LightData light) {
    float3 lightDirWS = normalize(light.direction);

    return CalculateLight(normalWS, viewDirWS, lightDirWS, light);
}

float3 CalculatePointLight(float3 positionWS, float3 normalWS, float3 viewDirWS, LightData light)
{     
    // Vector from fragment -> light
    float3 toLight = light.position - positionWS;
    float distanceToLight = length(toLight);

    // Outside light range
    if (distanceToLight >= light.range)
        return float3(0.0f, 0.0f, 0.0f);

    float3 lightDirWS = toLight / max(distanceToLight, 0.0001f);  // normalize

    float attenuation = saturate( 1.0f - distanceToLight / light.range);
    // smoother falloff
    attenuation *= attenuation;

    return CalculateLight(normalWS, viewDirWS, lightDirWS, light) * attenuation;
}

float3 CalculateSpotLight(float3 positionWS, float3 normalWS, float3 viewDirWS, LightData light)
{     
    // Vector from fragment -> light
    float3 toLight = light.position - positionWS;
    float distanceToLight = length(toLight);

    // Outside light range
    if (distanceToLight >= light.range)
        return float3(0.0f, 0.0f, 0.0f);

    float3 lightDirWS = toLight / max(distanceToLight, 0.0001f);  // normalize

    // DISTANCE ATTENUATION
    float distanceAttenuation  = saturate( 1.0f - distanceToLight / light.range);
    // smoother falloff
    distanceAttenuation  *= distanceAttenuation;
    
    // CONE ATTENUATION
    // Direction from light -> fragment
    float3 lightToFragment = -lightDirWS;
     // light.direction is the spotlight's forward direction
    float3 spotDirection = normalize(light.direction);
    // how close is the fragment to the center of the cone (spotDirection-vector)
    float cosAngle = dot( spotDirection, lightToFragment );

    // innerConeCos > outerConeCos
    //
    // inside inner cone  -> 1
    // outside outer cone -> 0
    // between            -> smooth transition

    float coneAttenuation =
        saturate(
            (cosAngle - light.outerConeCos) /
            max(light.innerConeCos - light.outerConeCos, 0.0001f)
        );

    // total attenuation
    float attenuation = distanceAttenuation * coneAttenuation;

    return CalculateLight(normalWS, viewDirWS, lightDirWS, light) * attenuation;
}

float2 ApplyParallaxOffset(float2 uv, float3 viewDirTS, float heightScale)
{
    float height = heightMap.Sample(textureSampler, uv).r;

    // centered height field: [0, 1] -> [-0.5, +0.5]
    height = (height - 0.5f) * heightScale;

    float viewZ = max(abs(viewDirTS.z), 0.05f);

    float2 offset = height * viewDirTS.xy / viewZ;

    return uv - offset;  // could be + instead of -
}

// Pixel/Fragment Shader
float4 PS(VOut input) : SV_TARGET
{
    float3 N = normalize(input.worldNormal);
    float3 T = normalize(input.tangentWS.xyz);
    float3 B = normalize(cross(N, T)) * input.tangentWS.w;

    // View Direction
    float3 viewDirWS = normalize( cameraPosition - input.worldPosition );
    float3 viewDirTS = normalize(
        float3( dot(viewDirWS, T), dot(viewDirWS, B), dot(viewDirWS, N) )
    );

    // PARALLAX MAPPING
    float2 uv = ApplyParallaxOffset( input.uv, viewDirTS, 0.03f );

    float3 normalTS = normalMap.Sample(textureSampler, uv).xyz * 2.0f - 1.0f;
    normalTS = normalize(normalTS);
    // Tangent Space -> World Space
    float3 normalWS = normalize( T * normalTS.x + B * normalTS.y + N * normalTS.z );

    float3 ambientLight = ambientColor * ambientIntensity;

    float3 lightColor = ambientLight;

    // iterate through all lights in the light buffer
    for (uint lightIdx = 0u; lightIdx < lightCount; lightIdx++)
    {
        LightData light = lights[lightIdx];
             
        if (light.type == 0)  // DIRECTIONAL LIGHTS
        {           
            lightColor += CalculateDirectionalLight(
                normalWS,
                viewDirWS,
                light
            );
        } else if (light.type == 1)  // POINT LIGHTS
        {           
            lightColor += CalculatePointLight(
                input.worldPosition,
                normalWS,
                viewDirWS,
                light
            );
        } else if (light.type == 2)  // SPOTLIGHTS
        {           
            lightColor += CalculateSpotLight(
                input.worldPosition,
                normalWS,
                viewDirWS,
                light
            );
        }
    }

    // sample texture and mix it with the material baseColor
    float4 textureColor = albedoMap.Sample(textureSampler, uv) * baseColor;
    
    return float4(textureColor.rgb * lightColor, textureColor.a);
}