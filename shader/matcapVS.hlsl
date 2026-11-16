#include "common.hlsl"

// マットキャップ用：画面から見た法線を PS に渡す
struct MC_OUT
{
    float4 pos : SV_POSITION;
    float3 viewNrm : NORMAL0;   // カメラから見た面の向き
};

MC_OUT main(in VS_IN input)
{
    MC_OUT output;

    matrix wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    output.pos = mul(input.pos, wvp);

    float3 worldNrm = normalize(mul(float4(input.nrm.xyz, 0.0f), World).xyz);
    output.viewNrm = normalize(mul(float4(worldNrm, 0.0f), View).xyz);
    return output;
}
