#include "lakeCommon.hlsl"

// lakeGrassPS : 草と葦の色。太陽の光・逆光で透ける光・月明かり・空からの光で照らす
struct PS_IN_GRASS
{
    float4 pos  : SV_POSITION;
    float4 col  : COLOR0;
    float2 tex  : TEXCOORD0;
    float3 wpos : TEXCOORD1;
    float3 nrm  : TEXCOORD2;
};

float4 main(in PS_IN_GRASS input) : SV_Target
{
    float3 toCam = camPos.xyz - input.wpos;
    float dist = length(toCam);
    float3 V = toCam / dist;
    float3 N = normalize(input.nrm);
    if (dot(N, V) < 0.0) N = -N;                  // 葉は両面

    float h01 = input.tex.x;
    float3 albedo = input.col.rgb * lerp(0.55, 1.1, h01);   // 先の方が明るい
    float ao = lerp(0.3, 1.0, h01);                          // 根元は他の葉の陰になって暗い

    float3 col = ShadeSurface(albedo, 0.45, N, V, 0.9) * ao;
    col = ApplyHaze(col, dist, -V);
    return float4(ToScreen(col), 1.0);
}
