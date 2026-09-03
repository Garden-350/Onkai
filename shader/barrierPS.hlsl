struct PS_IN_BARRIER
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

cbuffer BarrierBuffer : register(b0) // PS用スロットはVS側のb0-b5と別枠なので0でOK
{
    float progress;
    float padding0[3];
    float4 baseColor;
}

float2 HexCoords(float2 uv)
{
    float2 r = float2(1.0, 1.73205081);
    float2 h = r * 0.5;
    float2 a = fmod(uv, r) - h;
    float2 b = fmod(uv - h, r) - h;
    return dot(a, a) < dot(b, b) ? a : b;
}

float HexEdge(float2 gv)
{
    float2 p = abs(gv);
    float c = dot(p, normalize(float2(1.0, 1.73205081)));
    c = max(c, p.x);
    return c;
}

float4 main(PS_IN_BARRIER input) : SV_TARGET
{
    float2 centered = (input.tex - 0.5) * 2.0;
    float radius = length(centered);

    // 円形にクリップ
    float circleMask = smoothstep(1.0, 0.85, radius);
    if (circleMask <= 0.001)
        discard;

    // ヘックス格子の境界線
    float2 gv = HexCoords(centered * 2.0);  //三角形の大きさ　掛ける値が小さいほど大きい
    float hd = HexEdge(gv);
    float edge = 1.0 - smoothstep(0.0, 1.0, 1.0 - hd); //線の太さ

    // progressで中心から外へ広がるリング
    float ringFront = progress * 1.3;
    float ringWidth = 0.35;
    float ringMask = smoothstep(ringFront - ringWidth, ringFront, radius)
                    * (1.0 - smoothstep(ringFront, ringFront + 0.05, radius));

    float baseGlow = edge * 1.0;
    float ringGlow = edge * ringMask * 3.0;

    float intensity = (baseGlow + ringGlow) * circleMask;

    // 出現/消滅フェード
    float fade = smoothstep(0.0, 0.1, progress) * (1.0 - smoothstep(0.85, 1.0, progress));
    intensity *= fade;
    
    intensity = saturate(intensity);

    float3 color = intensity;
    return float4(color, intensity);
    //return float4(1.0,0.0,0.0,1.0);
}