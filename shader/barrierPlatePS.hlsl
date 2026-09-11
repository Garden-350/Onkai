#include "common.hlsl"

Texture2D barrierTex : register(t0);
SamplerState samp : register(s0);

cbuffer BarrierPlateParms : register(b6)
{
    float alpha;
    float3 padding0;
    float4 tintColor;
}

float4 main(in PS_IN input) : SV_TARGET
{
    float4 tex = barrierTex.Sample(samp, input.tex);
    float3 color = tex.rgb * tintColor.rgb;
    return float4(color, tex.a * alpha);
}