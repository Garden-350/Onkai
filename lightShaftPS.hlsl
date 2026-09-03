cbuffer LightShaftCB : register(b0)
{
	float4 lightOrigin;		//ライトシャフト発射位置
	float4 lightDirection;
	float intensity;		//ライトシャフトの強度
    float falloff;			 //ライトシャフトの減衰
	float samplingDensity;	//サンプリング回数
	float padding;
};

Texture2D mainTexture : register(t0);
SamplerState mainSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.texCoord;
	
	//ライト発射位置からの方向ベクトル
    float2 toLight = lightOrigin.xy - uv;
    float distToLight = length(toLight);
	
	//方向を正規化
    float2 lightDir = normalize(toLight);
	
	//サンプリング設定
    float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int samples = max(1, (int) samplingDensity);
	
	//ライト発射位置からピクセルへ向かう方向でレイマーチング
    [unroll(64)]
    for (int i = 0; i < samples; i++)
    {
        float t = float(i) / float(samples);
        
        //ライト発射位置から徐々にピクセルへ向かう
        float2 samplePos = lightOrigin.xy + (uv - lightOrigin.xy) * t;
        
        //スクリーン座標の範囲内に収める
        if(samplePos.x >= 0.0f && samplePos.x <= 1.0f &&
            samplePos.y >= 0.0f && samplePos.y <= 1.0f)
        {
            float4 sample = mainTexture.Sample(mainSampler, samplePos);
            
            //距離減衰
            float falloffFactor = exp(-t * t * falloff);
            
            result += sample * falloffFactor;
        }
    }
    //平均化
    result /= float(samples);
	//最終的な強度を適用    
    return result * intensity;
}