#include "common.hlsl"

Texture2D noiseTex : register(t0);
SamplerState samp : register(s0);

cbuffer BeamParams : register(b6)
{
    float time;
    float intensity;
    float2 padding0;
    float4 baseColor;
}

float4 main(in PS_IN input) : SV_TARGET
{
    float2 uv1 = input.tex + float2(0.0, time * 1.5);
    float2 uv2 = input.tex * 1.7 - float2(time * 0.8, time * 2.2);

    float n1 = noiseTex.Sample(samp, uv1).r;
    float n2 = noiseTex.Sample(samp, uv2).r;
    float noise = n1 * n2;

    float edge = 1.0 - abs(input.tex.x - 0.5) * 2.0;
    edge = saturate(edge);

    // ── 芯の発光を強める：edgeの指数を下げて広い範囲を明るくする ──
    float glow = pow(edge, 0.6) * (0.75 + 0.25 * noise);

    float3 color = baseColor.xyz * glow * intensity;

    // ── flickerは完全に0にせず、最低限のalphaを残す ──
    float flicker = lerp(0.5, 1.0, step(0.1, noise)); // 谷でも0.5は残す
    color *= flicker;

    // ── alphaの下限を持ち上げて、常にある程度は不透明にする ──
    float alpha = saturate(glow * flicker + 0.3);

    return float4(color, alpha);
}