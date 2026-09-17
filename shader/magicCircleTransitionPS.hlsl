#include "common.hlsl"

Texture2D circleTex : register(t0);
SamplerState samp : register(s0);

cbuffer MagicCircleParms : register(b6)
{
    float progress;
    float3 glowColor;
    float3 tintColor;
}

static const float PI = 3.14159265;

float4 main(in PS_IN input) : SV_TARGET
{
    float2 centered = input.tex - float2(0.5, 0.5);
    float dist = length(centered);
    if(dist > 0.5)
        discard;    //‰~‚ÌŠO‚Í“§–¾
    
    //‰E‚O@ã{
    float angle = atan2(-centered.y, centered.x);
    
    float u;    //¡‚Ç‚Ì‚ ‚½‚è‚É‚¢‚é‚©‚ğ01‚Å”»’è
    if(angle >= 0.0)
    {
        //ã”¼•ª•`‰æ
        u = -angle / PI;
    }
    else
    {
        //‰º”¼•ª
        u = -angle / PI;
    }
    
    if(u > progress)
        discard;    //•`‚©‚ê‚Ä‚È‚¢‚Æ‚±‚ë‚Í“§–¾
    
    float4 tex = circleTex.Sample(samp, input.tex);
    float3 baseColor = tex.rgb * tintColor;
    
    //ü‚Ìæ’[‚ğŒõ‚ç‚¹‚é
    float edgeGlow = smoothstep(progress - 0.06, progress, u);
    float3 color = baseColor + glowColor * edgeGlow * 1.5;
    
    return float4(color, tex.a);
}