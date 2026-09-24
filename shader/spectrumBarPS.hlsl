#include "common.hlsl"

// SpectrumRing 用。テクスチャは使わず、UVから形と光り方を計算する
cbuffer SpectrumParams : register(b6) // Shader::UpdatePSConstantBuffer はスロット6に送る
{
    float4 barColor;   // rgb=色, a=不透明度
    float  mode;       // 0=バー, 1=土台の円, 2=背景を暗くする円
    float  ringWidth;  // mode1用：円の線の太さ(UV単位)
    float2 padding;
}

float4 main(in PS_IN input) : SV_Target
{
    float2 uv = input.tex;

    if (mode < 0.5)
    {
        // ── バー ──
        // 横方向：中心ほど明るく、端はふわっと消える
        float x = abs(uv.x * 2.0 - 1.0);           // 0=中心, 1=端
        float core = saturate(1.0 - x);
        float glow = core * core;

        // 縦方向：uv.y=1 が根本、uv.y=0 が先端
        float h = 1.0 - uv.y;                       // 0=根本, 1=先端
        float tip = 1.0 - smoothstep(0.82, 1.0, h); // 先端を丸っぽく消す
        float root = smoothstep(0.0, 0.10, h);      // 根本もうっすら消す

        float a = glow * tip * root * barColor.a;

        // 芯を少し白く、先端ほど明るく
        float3 col = barColor.rgb * (0.7 + 0.6 * h) + pow(core, 6.0) * 0.35;
        return float4(col, a);
    }
    else if (mode < 1.5)
    {
        // ── 土台の円 ──
        float d = length(uv * 2.0 - 1.0);           // 0=中心, 1=クワッドの端
        float r0 = 1.0 - ringWidth * 3.0;           // 線の中心位置（端で切れないよう少し内側）
        float t = (d - r0) / ringWidth;
        float a = exp(-t * t) * barColor.a;
        return float4(barColor.rgb, a);
    }
    else
    {
        // ── 背景を暗くする円（中心ほど濃く、外側はふわっと消える）──
        float d = length(uv * 2.0 - 1.0);
        float a = (1.0 - smoothstep(0.55, 1.0, d)) * barColor.a;
        return float4(barColor.rgb, a);
    }
}
