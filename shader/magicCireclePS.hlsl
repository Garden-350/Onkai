#include "common.hlsl"

Texture2D circleTex : register(t0);
SamplerState samp : register(s0);

cbuffer MagicCirecleParms : register(b6)
{
    float progress; //0~1   1‚Å•`‚«I‚í‚è
    float3 glowColor;
    float3 tintColor;
}

float4 main(in PS_IN input) : SV_TARGET
{
    float2 centered = input.tex - float2(0.5, 0.5);
    float dist = length(centered);
    if (dist > 0.5)
        discard; //‰~‚ÌŠO‚Í“§–¾
    
    //UV‚©‚çŠp“x‚ğŒvZi12Ÿ•ûŒü‚ğ0‚Æ‚µ‚ÄŒv‰ñ‚è‚É‰ñ“]j
    float angle = atan2(centered.x, -centered.y);
    float angle01 = angle / (2.0 * 3.14159265) + 0.5;
    
    //progress‚æ‚èæ‚ÌŠp“x‚Í‚Ü‚¾•`‚©‚È‚¢
    if (angle01 > progress)
        discard;
    
    float4 tex = circleTex.Sample(samp, input.tex);
    float3 baseColor = tex.rgb * tintColor;
    
    //•`‚«I‚í‚Á‚½‹«ŠEü‚ğŒõ‚ç‚¹‚é
    float edgeGlow = smoothstep(progress - 0.05, progress, angle01);
    float3 color = tex.rgb + glowColor * edgeGlow * 1.5;
    
    return float4(color, tex.a);
}