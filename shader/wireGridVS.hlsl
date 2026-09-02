#include "common.hlsl"

PS_IN main(VS_IN input)
{
    PS_IN output;

    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    output.pos = mul(input.pos, wvp);
    output.col = input.col;
    output.tex = input.tex;

    return output;
}

