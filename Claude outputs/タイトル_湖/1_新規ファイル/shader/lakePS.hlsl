#include "lakeCommon.hlsl"

// ============================================================
// lakePS : タイトルの湖の「全画面」の部分。画面の1ピクセルごとに視線を飛ばして、
//   空（太陽・月・星・雲）/ 向こう岸の山と森 / 湖の水面 / 手前の岸の地面 を描く。
//   水面は PBR（GGX＋フレネル）で、空や山や森をちゃんと映す。風と突風で波立つ。
//   頂点シェーダーは FullscreenVS.hlsl（画面いっぱいの三角形）を使う。
//   ※実行時にコンパイルされるので、ここの数字を変えたらゲームを再起動するだけで反映される
// ============================================================

struct PS_IN_FULLSCREEN
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// ── 景色の大きさ（単位はだいたい cm）──
static const float kFarShore    = 30000.0;  // 向こう岸までの距離
static const float kCloudHeight = 3000.0;   // 雲の高さ（カメラから）

// ── 水 ──
static const float3 kDeepWater    = float3(0.004, 0.018, 0.022);  // 深い所の水の色
static const float3 kShallowWater = float3(0.030, 0.045, 0.028);  // 岸近くの浅い所（底が透けて見える）
static const float  kWaterF0      = 0.02;                         // 水の反射率（正面から見たとき）

// ------------------------------------------------------------
// 向こう岸の山と森（方角ごとの高さ。高さは視線のyで表す）
// ------------------------------------------------------------
float MountainFar(float az)  { return 0.035 + 0.110 * Fbm(float2(az * 1.6, 3.1)); }
float MountainNear(float az) { return 0.020 + 0.060 * Fbm(float2(az * 3.2 + 8.0, 1.7)); }
float TreeLine(float az)
{
    float groups = 0.012 + 0.012 * Noise1(az * 18.0) + 0.005 * Noise1(az * 60.0);
    float c = frac(az * 260.0 + Noise1(az * 40.0) * 3.0) * 2.0 - 1.0;          // 1本1本の木のこずえ
    float crown = sqrt(saturate(1.0 - c * c)) * (0.003 + 0.004 * Noise1(az * 130.0));
    return groups + crown;
}

// 空の色（太陽・月・星・雲）。withDisc=false のときは太陽と月の円盤を描かない（水面の反射用）
float3 Sky(float3 d, bool withDisc)
{
    float h = saturate(d.y);
    float3 col = lerp(skyHorizon.rgb, skyZenith.rgb, pow(h, 0.45));

    // 太陽のまわりの明るさ（夕方ほど地平線近くで広がる）
    float s = saturate(dot(d, sunDir.xyz));
    float nearHorizon = 1.0 - saturate(sunDir.y * 2.0);
    col += sunColor.rgb * (pow(s, 6.0) * (0.10 + 0.25 * nearHorizon) + pow(s, 80.0) * 0.5) * sunDir.w;
    if (withDisc)
        col += sunColor.rgb * smoothstep(0.99980, 0.99990, s) * 60.0 * sunDir.w;

    // 月
    float m = saturate(dot(d, moonDir.xyz));
    col += kMoonLight * moonDir.w * (pow(m, 300.0) * 0.6);
    if (withDisc)
        col += float3(1.0, 1.0, 0.95) * smoothstep(0.99988, 0.99993, m) * 3.0 * saturate(moonDir.w * 4.0);

    // 星（夜だけ）
    if (skyZenith.a > 0.01 && d.y > 0.0)
    {
        float2 g = d.xz / (d.y + 0.35) * 260.0;
        float2 id = floor(g);
        float rnd = Hash12(id);
        float2 off = float2(Hash12(id + 3.1), Hash12(id + 7.7)) - 0.5;
        float r = length(frac(g) - 0.5 - off * 0.6);
        float star = step(0.94, rnd) * (1.0 - smoothstep(0.02, 0.09, r));
        star *= 0.55 + 0.45 * sin(Time() * (2.0 + rnd * 4.0) + rnd * 60.0);      // またたき
        col += float3(0.9, 0.95, 1.0) * star * 1.6 * skyZenith.a * smoothstep(0.03, 0.25, d.y);
    }

    // 雲（高い所の平らな層。風で流れる）
    if (d.y > 0.01 && skyHorizon.a > 0.01)
    {
        float2 p = d.xz * (kCloudHeight / d.y) * 0.00035 + wind.xy * Time() * (0.004 + 0.006 * wind.w);
        float base = Fbm(p);
        float dens = saturate((base - (1.0 - skyHorizon.a)) * 2.4);
        float toward = Fbm(p + sunDir.xz * 0.06);                                // 太陽側の厚さ
        float lit = saturate(0.55 + (base - toward) * 3.0);
        float3 dark = skyZenith.rgb * 0.5 + ambientCol.rgb * 0.5 + kMoonLight * moonDir.w * 0.15;
        float3 bright = sunColor.rgb * 0.35 + skyHorizon.rgb * 0.5;
        float3 cloud = lerp(dark, bright, lit);
        cloud += sunColor.rgb * pow(s, 10.0) * 0.6 * (1.0 - dens);                // 太陽の近くの雲の縁が光る
        col = lerp(col, cloud, dens * 0.9 * smoothstep(0.01, 0.12, d.y));
    }
    return col;
}

// 空＋向こう岸の山と森。baseY は向こう岸の水際の高さ（視線のyで。カメラの高さぶん少し下）
float3 SkyAndLand(float3 d, float baseY, bool withDisc)
{
    float az = atan2(d.x, d.z);
    float y = d.y - baseY;

    float3 lightLand = ambientCol.rgb * 0.9 + sunColor.rgb * saturate(sunDir.y + 0.1) * 0.35 + kMoonLight * moonDir.w * 0.2;

    if (y < TreeLine(az))
    {
        float3 trees = float3(0.025, 0.05, 0.025) * lightLand;
        trees += sunColor.rgb * pow(saturate(dot(d, sunDir.xyz)), 20.0) * 0.05;   // 太陽側は木の輪郭が少し光る
        return lerp(trees, HazeColor(d), 0.25);
    }
    if (y < MountainNear(az))
        return lerp(float3(0.04, 0.06, 0.05) * lightLand, HazeColor(d), 0.30);
    if (y < MountainFar(az))
        return lerp(float3(0.05, 0.06, 0.08) * lightLand, HazeColor(d), 0.50);
    return Sky(d, withDisc);
}

// ------------------------------------------------------------
// 水面の傾き（波）
// ------------------------------------------------------------
float2 WaveSlope(float2 xz, float dist)
{
    float t = Time();
    float2 wd = wind.xy;
    float2 side = float2(-wd.y, wd.x);

    // うねり：いくつかの向きの波を重ねる（深い水の波の速さ ω=√(gk)、g=980cm/s^2）
    float2 slope = 0.0;
    float4 dirs[4] = {
        float4(wd, 180.0, 0.030),
        float4(normalize(wd + side * 0.6), 110.0, 0.028),
        float4(normalize(wd - side * 0.5), 70.0, 0.022),
        float4(normalize(wd + side * 1.4), 43.0, 0.016) };
    [unroll] for (int i = 0; i < 4; i++)
    {
        float k = 2.0 * PI / dirs[i].z;
        float w = sqrt(980.0 * k);
        float ph = dot(dirs[i].xy, xz) * k - w * t + i * 1.7;
        slope += dirs[i].xy * cos(ph) * dirs[i].w;
    }
    slope *= 0.6 + wind.z;

    // さざ波：風に流される細かいノイズ（突風の所だけ強くなる）
    float gust = GustAt(xz) + wind.w * 0.35 * smoothstep(0.45, 0.8, Noise(xz * 0.0012 - wd * t * 0.25));
    float2 q = xz * 0.03 - wd * t * 1.2;
    float e = 0.35;
    float n0 = Noise(q) + 0.5 * Noise(q * 2.7 + 5.0);
    float nx = Noise(q + float2(e, 0.0)) + 0.5 * Noise((q + float2(e, 0.0)) * 2.7 + 5.0);
    float nz = Noise(q + float2(0.0, e)) + 0.5 * Noise((q + float2(0.0, e)) * 2.7 + 5.0);
    slope += float2(nx - n0, nz - n0) / e * (0.04 + 0.08 * wind.z + 0.35 * gust);

    // 遠くの細かい波は見えないので弱める（チラつき防止）
    return slope / (1.0 + dist / 2500.0);
}

// ------------------------------------------------------------
float4 main(in PS_IN_FULLSCREEN input) : SV_Target
{
    // このピクセルの視線
    float2 ndc = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
    float3 d = normalize(camForward.xyz
                       + camRight.xyz * (ndc.x * camForward.w * camRight.w)
                       + camUp.xyz * (ndc.y * camForward.w));
    float3 cam = camPos.xyz;
    float3 V = -d;
    float3 col;

    // ── ① 手前の岸（地面）に当たる？ ──
    if (d.y < -1e-4 && cam.y > kBankY)
    {
        float t = (kBankY - cam.y) / d.y;
        float3 P = cam + d * t;
        if (P.z < ShoreZ(P.x))
        {
            float n = Noise(P.xz * 0.02) * 0.6 + Noise(P.xz * 0.11) * 0.4;
            float3 dirt = float3(0.10, 0.075, 0.05);
            float3 grass = float3(0.035, 0.07, 0.02);
            float3 albedo = lerp(dirt, grass, smoothstep(0.35, 0.65, n));
            float wet = 1.0 - smoothstep(0.0, 25.0, ShoreZ(P.x) - P.z);                  // 水際は湿って暗い
            albedo *= 1.0 - wet * 0.45;
            col = ShadeSurface(albedo, lerp(0.9, 0.35, wet), float3(0.0, 1.0, 0.0), V, 0.0);
            col = ApplyHaze(col, t, d);
            return float4(ToScreen(col), 1.0);
        }
    }

    // ── ② 水平線より上：空・向こう岸の山と森 ──
    float farBase = -cam.y / kFarShore;              // 向こう岸の水際が見える高さ
    if (d.y >= farBase)
    {
        col = SkyAndLand(d, farBase, true);
        return float4(ToScreen(col), 1.0);
    }

    // ── ③ 湖の水面 ──
    {
        float t = -cam.y / d.y;
        float3 P = cam + d * t;
        float2 slope = WaveSlope(P.xz, t);
        float3 N = normalize(float3(-slope.x, 1.0, -slope.y));
        float NoV = saturate(dot(N, V));

        // 映り込み：反射した視線の先の空・山・森（水面の高さから見る）
        float3 R = reflect(d, N);
        R.y = max(R.y, 0.002);
        R = normalize(R);
        float3 refl = SkyAndLand(R, 0.0, false);

        // 水の中の色：岸に近いほど浅くて底が透ける
        float shallow = 1.0 - smoothstep(0.0, 600.0, P.z - ShoreZ(P.x));
        float3 body = lerp(kDeepWater, kShallowWater, shallow);
        body *= ambientCol.rgb * 1.5 + sunColor.rgb * saturate(sunDir.y) * 0.6 + kMoonLight * moonDir.w * 0.3;

        float F = F_Schlick(kWaterF0, NoV);
        col = lerp(body, refl, F);

        // 太陽と月の光の道（波が立つほど広がる）
        float rough = 0.05 + 0.08 * wind.z + 0.15 * wind.w + t * 0.000003;
        col += sunColor.rgb * sunDir.w * SpecularGGX(N, V, sunDir.xyz, rough, kWaterF0) * 6.0;
        col += kMoonLight * moonDir.w * SpecularGGX(N, V, moonDir.xyz, rough, kWaterF0) * 4.0;

        // 水際の白い泡
        float edge = 1.0 - smoothstep(0.0, 10.0, P.z - ShoreZ(P.x));
        col += edge * 0.25 * (ambientCol.rgb + sunColor.rgb * 0.3) * (0.6 + 0.4 * Noise(P.xz * 0.2 + Time()));

        col = ApplyHaze(col, t, d);
    }
    return float4(ToScreen(col), 1.0);
}
