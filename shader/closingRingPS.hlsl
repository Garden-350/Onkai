#include "common.hlsl"

cbuffer ClosingRingParams : register(b6)
{
    float progress;
    float3 padding0;
    float4 ringColor;
}

static const float PI = 3.14159265f;
static const float HALF_PI = 1.57079632f;

float4 main(in PS_IN input) : SV_TARGET
{
    float2 centered = (input.tex - 0.5) * 2.0;
    float dist = length(centered);

    const float outerR = 1.0;
    const float innerR = 0.9;   //数字が大きいほど細い
    if (dist > outerR || dist < innerR)
        discard;

    float angle = atan2(centered.y, centered.x);
    angle -= HALF_PI;
    if (angle < -PI)
        angle += 2.0 * PI;
    if (angle > PI)
        angle -= 2.0 * PI;
    
    float distFromRight = abs(angle);
    float distFromLeft = PI - distFromRight;

    //float easedProgress = progress * progress * progress; // 3乗カーブ：序盤はゆっくり、終盤で一気に閉じる
    float grow = progress * HALF_PI;

    bool visibleRight = distFromRight <= grow;
    bool visibleLeft = distFromLeft <= grow;
    if (!visibleRight && !visibleLeft)
        discard;

    float edge = min(abs(distFromRight - grow), abs(distFromLeft - grow));
    float glow = smoothstep(0.10, 0.0, edge);

    float3 color = ringColor.rgb + glow * 0.6;
    return float4(color, 1.0);
}