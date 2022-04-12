cbuffer FScb : register(b0)
{
    uint   currentLevel;
    uint3  channels;
};

Texture2D gTexture;
SamplerState gSampler;

struct FsIn
{
    float4 posH : SV_POSITION;
    float2 texC : TEXCOORD;
    float3 posW : POSITION;
    float3 norm : NORMAL;
};

float4 mainDebug(FsIn fs) : SV_TARGET
{
    float4 c = gTexture.SampleLevel( gSampler, fs.texC, currentLevel );
    float arr[8] = { c.x, c.y, c.z, c.w, 0.0, 0.5, 1.0, 0.0 };
    uint3 ch = channels % 8;
    return float4(arr[ch.x], arr[ch.y], arr[ch.z], 1.0);
}
