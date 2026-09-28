#include "common.hlsl"

// TitleCorridor 用。「たどり着けない回廊」の床・壁・天井の線、奥の壁、出口の扉をまとめて描く。
// 線はテクスチャを使わず、板の上の座標から計算する。
Texture2D g_Texture : register(t0);         // 扉の絵（mode 2 のときだけ使う）
SamplerState g_SamplerState : register(s0);

cbuffer CorridorParams : register(b6) // Shader::UpdatePSConstantBuffer はスロット6に送る
{
    float4 lineColor;   // rgb=線の色, a=線の明るさ
    float4 fillColor;   // rgb=面の色
    float4 fogColor;    // rgb=霧の色, a=霧の最大の濃さ
    float4 doorColor;   // rgb=扉からもれる光の色, a=その強さ
    float  mode;        // 0=床・壁・天井 / 1=奥の壁 / 2=扉
    float  sizeU;       // 板の横の長さ（ワールド単位）
    float  sizeV;       // 板の縦の長さ（ワールド単位）
    float  zNear;       // 板の手前の端のz（奥の壁・扉ならその板のz）
    float  scroll;      // 歩いた距離（横線を手前へ流す量）
    float  cell;        // マス目の大きさ
    float  lineWidth;   // 線の太さ
    float  zDoor;       // 扉のz
    float  fogStart;    // 霧がかかり始める距離
    float  fogEnd;      // 霧が一番濃くなる距離
    float  doorW;       // 扉の幅
    float  doorH;       // 扉の高さ
    float  openAmount;  // 0～1 扉から光があふれる量
    float  revealDist;  // この距離より手前の線だけ光る（起動演出用）
    float  padding0;
    float  padding1;
}

// c : マス目単位の座標 / fw : 1ピクセルあたり何マス進むか / halfW : 線の半分の太さ（マス目単位）
// 戻り値 x=線そのもの, y=線のまわりのぼんやりした光
float2 GridLine(float c, float fw, float halfW)
{
    float d = abs(frac(c + 0.5) - 0.5);            // 一番近い線までの距離
    float w = max(fw, 1e-5);
    float drawW = max(halfW, w * 0.5);             // 1ピクセルより細くはしない
    float ln = 1.0 - smoothstep(drawW - w * 0.5, drawW + w * 0.5, d);
    ln *= halfW / drawW;                           // 太らせたぶん暗くして、見た目の量を保つ
    float halo = exp(-d * d / 0.004) * (1.0 - saturate(w * 4.0));

    // 遠くでマス目が細かすぎる所は、平均の明るさに寄せてチラつき(モアレ)を防ぐ
    float toAverage = saturate(w * 2.0 - 0.5);
    ln = lerp(ln, halfW * 2.0, toAverage);
    halo *= 1.0 - toAverage;
    return float2(ln, halo);
}

float4 main(in PS_IN input) : SV_Target
{
    float2 uv = input.tex;

    // ── 板の上での座標（ワールド単位）と、カメラからの奥行き ──
    float2 coord;
    float dist;
    if (mode < 0.5)
    {
        float along = (1.0 - uv.y) * sizeV;            // 手前の端からの距離（uv.y=0 が奥）
        dist = zNear + along;
        coord = float2(uv.x * sizeU, dist + scroll);   // 横線は歩いたぶんだけ手前へ流れる
    }
    else
    {
        dist = zNear;
        coord = float2(uv.x * sizeU, (1.0 - uv.y) * sizeV); // 奥の壁・扉：x=左からの位置, y=床からの高さ
    }

    float2 gc = coord / cell;
    float2 fw = fwidth(gc);
    float halfW = lineWidth * 0.5 / cell;

    float fog = smoothstep(fogStart, fogEnd, dist) * fogColor.a;
    float reveal = 1.0 - smoothstep(revealDist - 150.0, revealDist, dist);
    float3 lightCol = doorColor.rgb * doorColor.a;
    float3 col;

    if (mode < 1.5)
    {
        // ── 床・壁・天井・奥の壁：光るマス目 ──
        float2 gx = GridLine(gc.x, fw.x, halfW);
        float2 gy = GridLine(gc.y, fw.y, halfW);
        float ln = max(gx.x, gy.x);
        float halo = max(gx.y, gy.y);

        col = fillColor.rgb + lineColor.rgb * lineColor.a * (ln + halo * 0.3) * reveal;
        col = lerp(col, fogColor.rgb, fog);

        if (mode < 0.5)
        {
            // 扉の光が、扉に近い床・壁・天井を照らす
            float toDoor = max(zDoor - dist, 0.0);
            col += lightCol * exp(-toDoor / 220.0) * 0.35;
        }
        else
        {
            // 奥の壁：扉のまわりににじむ光
            float dx = max(abs(coord.x - sizeU * 0.5) - doorW * 0.5, 0.0);
            float dy = max(coord.y - doorH, 0.0);
            float d = length(float2(dx, dy));
            col += lightCol * (exp(-d / 45.0) * 0.9 + exp(-d / 160.0) * 0.25);
        }

        // 扉が開くと、回廊全体が奥から白い光に満たされていく
        col += float3(1.0, 1.0, 1.0) * openAmount * openAmount * 0.6 * exp(-max(zDoor - dist, 0.0) / 400.0);
    }
    else
    {
        // ── 出口の扉 ──
        float4 tex = g_Texture.SampleLevel(g_SamplerState, uv, 0);
        float2 c = uv * 2.0 - 1.0;
        float3 panel = fillColor.rgb * 2.5 + lightCol * 0.12 * (1.0 - 0.5 * dot(c, c)); // 扉の板（中心ほどほんのり明るい）
        float3 ink = tex.rgb * (0.3 + 0.9 * doorColor.a);                              // 枠と「出口」の文字（扉の光と一緒に明るくなる）
        col = lerp(panel, ink, tex.a);
        col = lerp(col, fogColor.rgb, fog * 0.35);          // 扉は霧に埋もれにくくする
        col = lerp(col, float3(1.3, 1.3, 1.3), openAmount); // 開くほど白い光で満たされる
    }

    return float4(col, 1.0);
}
