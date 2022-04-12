cbuffer CScb : register(b0)
{
    uint2 maxSize;
};

Texture2D<float4> src;
RWTexture2D<float4> dst;

#define DST dst[threadId.xy]

#ifndef ASSIGN
#define ASSIGN DST.xyzw = SRC.xyzw;
#endif

float4 centralDiff(uint2 id, int2 dd, float one_over_eps)
{
    float4 diff = src.Load(int3((id + dd + maxSize) % maxSize, 0)) - src.Load(int3((id - dd + maxSize) % maxSize, 0));
    return 0.5 * diff * one_over_eps;
}

float2 getNormal(uint2 id)
{
    float dx = centralDiff(id, int2(1, 0), (float) maxSize.x).x;
    float dy = centralDiff(id, int2(0, 1), (float) maxSize.y).x;
    float2 ret = normalize(float3(-dx, -dy, 1)).xy;
    return ret*.5 + .5;
}

[numthreads(16, 16, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId.xy >= maxSize)) return;

    float4 SRC = src.Load(int3(threadId.xy, 0)).xyzw;
    
    ASSIGN
}
