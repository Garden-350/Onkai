#include "common.hlsl"

// 光の帯用の頂点シェーダー：UVはそのまま渡す（SetUVの影響を受けない）
PS_IN main(in VS_IN input)
{
	PS_IN output;
	matrix wvp = mul(World, View);
	wvp = mul(wvp, Projection);
	output.pos = mul(input.pos, wvp);
	output.col = input.col;
	output.tex = input.tex;
	return output;
}
