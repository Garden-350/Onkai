#include "common.hlsl"

Texture2D noiseTex : register(t0);
SamplerState samp : register(s0);

cbuffer LongBeamParams : register(b6)
{
	float4 glowColor;	// 外側の柔らかい光の色
	float intensity;	// 全体の明るさ
	float time;			// 模様を流す時間
	float2 padding;
}

// tex.x : 帯の横方向（0=左端 0.5=中心 1=右端）
// tex.y : 線に沿った距離（手前が0、奥ほど大きい）
float4 main(in PS_IN input) : SV_TARGET
{
	float d = abs(input.tex.x - 0.5) * 2.0;	// 中心0～端1

	float core = exp(-d * d * 60.0);		// 細く強い芯
	float glow = exp(-d * d * 5.0);			// 太く柔らかい光

	// 線に沿って、奥から手前へ流れるエネルギーの揺らぎ
	float2 uv = float2(input.tex.x * 0.6, input.tex.y * 0.12 + time * 1.5);
	float n = noiseTex.Sample(samp, uv).r;
	float energy = 0.7 + 0.6 * n;

	float3 coreColor = lerp(input.col.rgb, float3(1.0, 1.0, 1.0), 0.6);	// 芯は白寄りのレーン色
	float3 color = coreColor * core * 1.8 + glowColor.rgb * glow * 0.8 * energy;
	color *= intensity;

	// 加算合成（色 × α）で使う。αは奥のフェード用
	return float4(color, input.col.a);
}
