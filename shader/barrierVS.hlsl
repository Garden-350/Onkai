#include "common.hlsl"

struct PS_IN_BARRIER
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PS_IN_BARRIER main(in VS_IN input)
{
    PS_IN_BARRIER output;

    matrix wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    output.pos = mul(input.pos, wvp);

    output.tex = input.tex;
    return output;
}