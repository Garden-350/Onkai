cbuffer SkyParms : register(b0)
{
    float time;
    float timeOfDay;
    float2 padding;
}

Texture2D gradientTex : register(t0);
Texture2D cloudTex : register(t1);
SamplerState samp : register(s0);

struct PSINPUT
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 dir : TEXCOORD1;
};

float4 main(PSINPUT input) : SV_TARGET
{
    float3 dir = normalize(input.dir);
    float h = saturate(dir.y * 0.5 + 0.5);

    float3 skyColor = gradientTex.Sample(samp, float2(0.5, 1.0 - h)).rgb;

    float angle = atan2(dir.z, dir.x) / 6.2831853 + 0.5;
    float2 cloudUV = float2(angle + time * 0.003, saturate(h * 2.0));
    float4 cloud = cloudTex.Sample(samp, cloudUV);

    cloud.a = cloud.a * saturate(1.0 - h * 1.4);

    float3 finalColor = lerp(skyColor, cloud.rgb, cloud.a);
    return float4(finalColor, 1.0);
}