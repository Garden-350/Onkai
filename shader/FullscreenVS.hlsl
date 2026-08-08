struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT main(uint id : SV_VertexID)
{
    VS_OUT o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}