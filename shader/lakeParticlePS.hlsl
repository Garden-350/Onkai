#include "common.hlsl"

// lakeParticlePS : 突風で舞う葉っぱ（mode 0）と、夜のホタル（mode 1）。
//   色（明るさ込み）はC++側で時間帯に合わせて計算して渡す
cbuffer ParticleParams : register(b6)
{
    float4 color;   // rgb=色（明るさ込み）, a=不透明度
    float4 misc;    // x=種類(0=葉 1=ホタル), y=露出
}

float3 ToScreenP(float3 c)
{
    float3 x = max(c, 0.0) * misc.y;
    x = saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
    return pow(x, 1.0 / 2.2);
}

float4 main(in PS_IN input) : SV_Target
{
    float2 p = input.tex * 2.0 - 1.0;   // -1～1

    if (misc.x < 0.5)
    {
        // 葉っぱ：縦長の葉の形 ＋ 真ん中の葉脈
        float halfW = 0.55 * sqrt(saturate(1.0 - p.y * p.y)) * (1.0 - 0.25 * p.y);
        float edge = halfW - abs(p.x);
        float a = smoothstep(0.0, 0.08, edge) * step(abs(p.y), 0.98);
        float vein = 1.0 - smoothstep(0.0, 0.06, abs(p.x)) * 0.35;
        float3 c = color.rgb * lerp(0.75, 1.0, vein) * (0.85 + 0.15 * p.y);
        if (a <= 0.01) discard;
        return float4(ToScreenP(c), a * color.a);
    }

    // ホタル：中心が明るく、まわりにぼんやり光が広がる（加算合成で描く）
    float r2 = dot(p, p);
    float glow = exp(-r2 * 18.0) * 2.0 + exp(-r2 * 4.0) * 0.35;
    return float4(ToScreenP(color.rgb * glow) * color.a, 1.0);
}
