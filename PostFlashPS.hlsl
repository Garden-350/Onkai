Texture2D screenTex : register(t0);
SamplerState samp : register(s1);

cbuffer FlashBuffer : register(b0)
{
    float4 flashColor;
    float flashAlpha;
    float3 padding;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VS_OUT input) : SV_TARGET
{
    float4 screenColor = screenTex.Sample(samp, input.uv);
    
    //加算合成でフラッシュをかける（screenブレンドにしたい場合はlerpに変更可）
    //float3 result = screenColor.rgb + flashColor.rgb * flashAlpha;
    float3 result = 1.0f - (1.0f - screenColor.rgb) * (1.0f - flashColor.rgb * flashAlpha);
    return float4(result, screenColor.a);
}