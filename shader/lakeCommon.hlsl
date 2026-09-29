// ============================================================
// lakeCommon.hlsl : タイトルの湖（TitleLake）のシェーダーで共通に使うもの。
//   ・時間帯・風・カメラの情報（LakeParams）
//   ・ノイズ、光の計算、霧、トーンマッピング
//   ※C++側の TitleLake::LakeParams と同じ並びにしておくこと
// ============================================================

cbuffer LakeParams : register(b6)
{
    float4 camPos;      // xyz=カメラの位置, w=経過秒
    float4 camForward;  // xyz=カメラの前, w=tan(縦の視野角/2)
    float4 camRight;    // xyz=カメラの右, w=画面の横/縦
    float4 camUp;       // xyz=カメラの上, w=露出（全体の明るさ）
    float4 sunDir;      // xyz=太陽のある方向, w=太陽の円盤の見え具合(0～1)
    float4 sunColor;    // rgb=太陽の光（色と強さ）, a=夜の度合い(0=昼 1=夜)
    float4 moonDir;     // xyz=月のある方向, w=月の光の強さ
    float4 skyZenith;   // rgb=真上の空の色, a=星の量
    float4 skyHorizon;  // rgb=地平線の空の色, a=雲の量
    float4 ambientCol;  // rgb=空からの光, a=霧の濃さ
    float4 wind;        // xy=風が吹いていく向き(xz平面), z=ふだんの風の強さ, w=突風の強さ(0～1)
    float4 misc;        // x=突風の先頭の位置, y=手前の岸までの距離, z=飛び込みの進み具合, w=未使用
}

static const float PI = 3.14159265;
static const float kBankY = 8.0;        // 手前の岸（地面）の高さ。水面は y=0

float Time() { return camPos.w; }

// ------------------------------------------------------------
// ノイズ
// ------------------------------------------------------------
float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float Noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = Hash12(i);
    float b = Hash12(i + float2(1.0, 0.0));
    float c = Hash12(i + float2(0.0, 1.0));
    float d = Hash12(i + float2(1.0, 1.0));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float Fbm(float2 p)
{
    float v = 0.0;
    float amp = 0.5;
    [unroll] for (int i = 0; i < 5; i++)
    {
        v += Noise(p) * amp;
        p = p * 2.03 + float2(17.1, 9.3);
        amp *= 0.5;
    }
    return v;
}

// 1次元のなめらかなノイズ（山の稜線など）
float Noise1(float x)
{
    return Noise(float2(x, 0.37));
}

// 手前の岸の線（z）。C++側の TitleLake::ShoreZ と同じ式
float ShoreZ(float x)
{
    return misc.y + 40.0 * sin(x * 0.004 + 1.3) + 18.0 * sin(x * 0.013 + 0.4);
}

// 突風の強さ（場所ごと）。先頭が通り過ぎた場所から順に強くなる
float GustAt(float2 xz)
{
    float behind = misc.x - dot(xz, wind.xy);
    float band = smoothstep(-200.0, 0.0, behind) * (1.0 - smoothstep(700.0, 2000.0, behind));
    float n = Noise(xz * 0.006 - wind.xy * Time() * 0.8);
    return wind.w * band * (0.55 + 0.9 * n);
}

// ------------------------------------------------------------
// 光
// ------------------------------------------------------------
float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float V_SmithGGX(float NoV, float NoL, float a)
{
    float a2 = a * a;
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (gv + gl + 1e-5);
}

float F_Schlick(float f0, float VoH)
{
    return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0);
}

// 鏡面反射の強さ（光の強さは掛けていない）
float SpecularGGX(float3 N, float3 V, float3 L, float rough, float f0)
{
    float a = max(rough * rough, 0.002);
    float3 H = normalize(L + V);
    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V)) + 1e-4;
    return D_GGX(saturate(dot(N, H)), a) * V_SmithGGX(NoV, NoL, a) * F_Schlick(f0, saturate(dot(V, H))) * NoL;
}

static const float3 kMoonLight = float3(0.55, 0.65, 0.9);

// 草・地面などの「ふつうの物」の明るさ。translucency は薄い葉が逆光で透ける量
float3 ShadeSurface(float3 albedo, float rough, float3 N, float3 V, float translucency)
{
    float3 L = sunDir.xyz;
    float NoL = saturate(dot(N, L));
    float3 col = albedo * sunColor.rgb * NoL;
    col += sunColor.rgb * SpecularGGX(N, V, L, rough, 0.04) * PI;

    // 逆光で透ける（太陽の方を向いて見たとき、葉が光る）
    float back = saturate(dot(-V, L));
    col += albedo * sunColor.rgb * translucency * (pow(back, 6.0) * 2.0 + 0.25) * saturate(dot(-N, L) + 0.6);

    // 月明かり
    float3 moon = kMoonLight * moonDir.w;
    col += albedo * moon * saturate(dot(N, moonDir.xyz) * 0.6 + 0.4);

    // 空からの光（上を向いている面ほど多い）
    col += albedo * ambientCol.rgb * (0.55 + 0.45 * N.y);
    return col;
}

// ------------------------------------------------------------
// 空気（遠くほど地平線の空の色に溶ける）
// ------------------------------------------------------------
float3 HazeColor(float3 dir)
{
    float s = saturate(dot(dir, sunDir.xyz));
    return skyHorizon.rgb + sunColor.rgb * pow(s, 6.0) * 0.12;
}

float3 ApplyHaze(float3 col, float dist, float3 dir)
{
    float f = 1.0 - exp(-dist * ambientCol.a * 0.00002);
    return lerp(col, HazeColor(dir), f);
}

// ------------------------------------------------------------
// 仕上げ：露出 → トーンマッピング(ACES) → 画面用のガンマ
// ------------------------------------------------------------
float3 ToScreen(float3 col)
{
    float3 x = max(col, 0.0) * camUp.w;
    x = saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
    return pow(x, 1.0 / 2.2);
}
