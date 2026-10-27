#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

//敵を一瞬光らせる
cbuffer EnemyFlash : register(b6)
{
    float4 flashColor;   //rgb:変える色　a:どれだけ変えるか
}

float main(in PS_IN input) : SV_Target
{
    float4 color;
    if (Material.TextureEnable)
    {
        color = g_Texture.Sample(g_SamplerState, input.tex);
        color *= input.col;
    }
    else
    {
        color = input.col * Material.Diffuse;
    }

	// 元の色と光らせたい色を、flashColor.a の割合で混ぜる
    color.rgb = lerp(color.rgb, flashColor.rgb, flashColor.a);
    return color;
}