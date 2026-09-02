#include "common.hlsl"

cbuffer BarrierBuffer : register(b4)
{
    float progress;
    float3 padding0;
    float4 baseColor;
}

float4 main(in PS_IN input) : SV_TARGET
{
    float2 centered = input.tex - 0.5f;
    float dist = length(centered) * 2.0;
    
    float ringRadius = progress;
    float ring1 = smoothstep(0.06, 0.0, abs(dist - ringRadius));
    float ring2 = smoothstep(0.02, 0.0, abs(dist - ringRadius * 0.75));
    
    float angle = atan2(centered.y, centered.x);
    float hexPattern = abs(sin(angle * 6.0 + progress * 3.14)) * 0.5 + 0.5;
    float geo = smoothstep(0.35, 0.0, abs(dist - ringRadius * 0.9)) * hexPattern;
    
    float coreGlow = smoothstep(0.5, 0.0, dist) * (1.0 - progress) * 0.6;
    
    float alphaMask = ring1 + ring2 * 0.7 + geo * 0.5 + coreGlow;
    float fade = 1.0 - smoothstep(0.6, 1.0, progress);
    
    float3 color = baseColor.xyz * alphaMask;
    float alpha = saturate(alphaMask * fade);
    
    return float4(color, alpha);
}