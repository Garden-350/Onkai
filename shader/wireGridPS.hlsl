#include "common.hlsl"

float4 main(PS_IN input) : SV_TARGET
{
    return input.col;
}
