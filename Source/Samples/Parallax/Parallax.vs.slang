cbuffer VScb : register(b1)
{
    float4x4 viewProj;
    float4x4 model;
    float4x4 modelIT;
};

struct VsIn
{
    float4 pos : POSITION;
    float2 tex : TEXCOORD;
    float3 norm : NORMAL;
};

struct VsOut
{
    float4 posH : SV_POSITION;
    float2 texC : TEXCOORD;
    float3 posW : POSITION;
    float3 norm : NORMAL;
};

VsOut main(VsIn vs)
{
    VsOut vOut;
    vOut.texC = vs.tex;
    vOut.posW = mul(vs.pos, model).xyz;
    vOut.posH = mul(float4(vOut.posW, 1), viewProj);
    vOut.norm = mul(float4(vs.norm, 0), modelIT).xyz;
    return vOut;
}
