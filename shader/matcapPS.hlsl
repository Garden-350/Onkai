#include "common.hlsl"

Texture2D g_Texture : register(t0);         // マットキャップの画像（金属の球）
SamplerState g_SamplerState : register(s0);

struct MC_IN
{
    float4 pos : SV_POSITION;
    float3 viewNrm : NORMAL0;
};

float4 main(in MC_IN input) : SV_Target
{
    float3 n = normalize(input.viewNrm);
    // 面の向きの x, y で、球の画像のどこを使うかを決める（右向きの面 = 球の右側、上向きの面 = 球の上側）
    float2 uv = float2(n.x, -n.y) * 0.49f + 0.5f;
    float3 col = g_Texture.Sample(g_SamplerState, uv).rgb;
    return float4(col, 1.0f);
}
