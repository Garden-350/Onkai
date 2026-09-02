cbuffer WorldBuffer : register(b0)
{
    matrix World;
};
cbuffer ViewBuffer : register(b1)
{
    matrix View;
};
cbuffer ProjBuffer : register(b2)
{
    matrix Proj;
};

struct VSINPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 dir : TEXCOORD1;
};

VSOutput main(VSINPUT input)
{
    VSOutput o;
    float4 worldPos = mul(float4(input.pos, 1.0f), World);
    o.worldPos = worldPos.xyz;
    o.dir = normalize(input.pos);
    o.pos = mul(mul(worldPos, View), Proj);
    o.pos.z = o.pos.w * 0.9999f;
    return o;
}