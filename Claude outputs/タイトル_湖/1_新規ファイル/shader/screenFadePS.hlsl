#include "common.hlsl"

// ScreenFade 用。画面全体を1色で塗るだけ（a で濃さを決める）
cbuffer FadeParams : register(b6) // Shader::UpdatePSConstantBuffer はスロット6に送る
{
    float4 fadeColor; // rgb=色, a=濃さ(0=見えない, 1=塗りつぶし)
}

float4 main(in PS_IN input) : SV_Target
{
    return fadeColor;
}
