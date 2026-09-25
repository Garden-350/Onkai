#include "common.hlsl"

cbuffer GuideParms : register(b6)
{
    float4 guideColor;  //rgb=色　a=明るさ
    float mode;         //0=ライン  1=トラック  2=バッド
    float param1;       //mode1:帯の中で判定円がある位置　mode2:押し込み具合(0~1)
    float param2;
    float padding;
}

float4 main(in PS_IN input) : SV_TARGET
{
    float2 uv = input.tex;
    float intensity = guideColor.a;
    float3 col = guideColor.rgb;
    float a = 0.0;
    
    if(mode < 0.5)
    {
        //横方向にぼかした帯
        float x = abs(uv.x * 2.0 - 1.0);    //0=中心　1=端
        a = exp(-x * x * 4.0) * intensity;
        col += pow(saturate(1.0 - x), 6.0) * 0.4;   //芯が白
    }
    else if(mode < 1.5)
    {
        //緑が光床の帯　奥ほど消える
        float x = abs(uv.x * 2.0 - 1.0);    //0=帯の中心 1=帯の緑
        float h = 1.0 - uv.y;               //0=手前の端　1=奥の端
        float edge = smoothstep(0.78, 0.94, x) * (1.0 - smoothstep(0.94, 1.0, x));  //緑の細い線
        float fill = 0.22 * (1.0 - x * x);  //中の方は薄い
        float farFade = pow(saturate(1.0 - h), 1.3);    //奥は暗く
        float nearFade = smoothstep(0.0, param1, h);    //判定円より手前ははみ出し分だけ消す
        a = (edge * 0.9 + fill) * farFade * nearFade * intensity;
    }
    else
    {
        //バッド：判定円の位置の光る輪。押すと中が光よ
        float d = length(uv * 2.0 - 1.0);   //0=中心　1=板の端
        float press = param1;
        float ring = exp(-pow((d - 0.78) / 0.07, 2.0));
        float fill = (1.0 - smoothstep(0.0, 0.8, d)) * press;
        float halo = exp(-d * d * 3.0) * 0.25 * press;
        a = (ring * (0.6 + 0.4 * press) + fill * 0.6 + halo) * intensity;
        col = lerp(col, float3(1.0, 1.0, 1.0), fill * 0.5);
    }
    //明るさが1を超える分は色を強くして表現（加算合成なので白飛び寄りに光るので気に入らなければ変える）
    return float4(col * max(1.0, a), saturate(a));
}