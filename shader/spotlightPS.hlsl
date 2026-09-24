#include "common.hlsl"

// SpotlightRow 用。テクスチャは使わず、UVから光の柱とランプを計算する
cbuffer SpotlightParams : register(b6) // Shader::UpdatePSConstantBuffer はスロット6に送る
{
    float4 lightColor;      // rgb=色, a=明るさ
    float  mode;            // 0=光の柱, 1=根本のランプ
    float  baseWidthRatio;  // 根本の幅 / 先端の幅（柱を末広がりにする）
    float2 padding;
}

float4 main(in PS_IN input) : SV_Target
{
    float2 uv = input.tex;
    float intensity = lightColor.a;

    if (mode < 0.5)
    {
        // ── 光の柱 ──
        float h = 1.0 - uv.y;                                  // 0=根本, 1=先端
        float halfW = lerp(baseWidthRatio, 1.0, h);            // この高さでの柱の幅
        float x = abs(uv.x * 2.0 - 1.0) / halfW;               // 0=中心, 1=柱の縁
        if (x >= 1.0) discard;

        float edge = 1.0 - x;
        float core = pow(edge, 1.5);                           // 縁ほどふわっと消える

        float fade = pow(1.0 - h, 1.8);                        // 上に行くほど消える
        float hot = exp(-h * 18.0) * 0.6;                      // 根本付近だけ強く光る
        float start = smoothstep(0.0, 0.02, h);                // 根本の切れ目を目立たなくする

        float a = saturate(core * (fade + hot) * start * intensity * 0.6);
        float3 col = lightColor.rgb + pow(edge, 8.0) * 0.3 * (1.0 - h); // 芯を少し白っぽく
        return float4(col, a);
    }
    else
    {
        // ── 根本のランプ（丸い光）──
        float d = length(uv * 2.0 - 1.0);
        float a = saturate(exp(-d * d * 5.0) * intensity);
        float3 col = lerp(lightColor.rgb, float3(1.0, 1.0, 1.0), exp(-d * d * 30.0));
        return float4(col, a);
    }
}
