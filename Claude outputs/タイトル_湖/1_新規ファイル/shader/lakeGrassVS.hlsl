#include "common.hlsl"
#include "lakeCommon.hlsl"

// ============================================================
// lakeGrassVS : 岸の草と葦。風でなびき、突風が来ると波のように倒れる。
//   頂点データの使い方（LakeGrass.cpp で作る）
//     position : 風が無いときの位置（ワールド座標。World行列は使わない）
//     normal   : 葉の面の向き
//     color    : rgb=葉の色, a=その葉ごとのばらつき(0～1)
//     uv       : x=根元からの高さの割合(0=根元 1=先), y=葉の長さ
// ============================================================

struct VS_OUT_GRASS
{
    float4 pos  : SV_POSITION;
    float4 col  : COLOR0;
    float2 tex  : TEXCOORD0;
    float3 wpos : TEXCOORD1;
    float3 nrm  : TEXCOORD2;
};

VS_OUT_GRASS main(in VS_IN input)
{
    VS_OUT_GRASS o;
    float3 p = input.pos.xyz;
    float h01 = input.tex.x;
    float len = input.tex.y;
    float rnd = input.col.a;
    float t = Time();

    float gust = GustAt(p.xz);
    // ゆっくり揺れる（場所ごとに少しずつずれて、風の波が草原を渡っていく）
    float sway = sin(t * 1.6 + rnd * 6.283 + dot(p.xz, wind.xy) * 0.025) * (0.25 + wind.z * 0.6);
    // 細かく震える（突風のときほど強い）
    float flutter = sin(t * 9.0 + rnd * 40.0) * 0.06 * (wind.z + gust * 1.5);
    float bend = wind.z * 0.35 + gust * 1.2 + sway * 0.35 + flutter;

    float k = h01 * h01;                                   // 先ほど大きく動く
    float2 w = wind.xy;
    p.xz += w * (bend * k * len * 0.55);
    p.y  -= bend * bend * k * len * 0.16;                  // 曲がったぶん少し低くなる

    o.wpos = p;
    o.pos = mul(mul(float4(p, 1.0), View), Projection);
    o.nrm = normalize(input.nrm.xyz + float3(w.x, 0.0, w.y) * bend * 0.3 + float3(0.0, 0.35, 0.0));
    o.col = input.col;
    o.tex = input.tex;
    return o;
}
