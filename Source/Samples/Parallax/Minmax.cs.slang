cbuffer CScb : register(b0)
{
    uint currentLevel;
    uint2 maxSize;
};

Texture2D<float> heightMap; // scalar valued
Texture2D<float2> srcMinmaxMap; // float2 valued, storing [min, max] - the finer LOD
RWTexture2D<float2> dstMinmaxMap; // float2 valued, storing [min, max] - destination LOD

// This function copies the most granular LOD (=0) of the heightmap to LOD 0 
// of the min-max heightmap. 
[numthreads(16, 16, 1)]
void copyFromScalarToVector(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId.xy >= maxSize)) return;

    float h = heightMap.Load(int3(threadId.xy, 0)).r;
    // store LOD is set implicitly by assigning the UAV of the destination LOD
    dstMinmaxMap[threadId.xy] = float2(h, h);
}

[numthreads(16, 16, 1)]
void mipmapMinmax(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId.xy >= maxSize)) return;
    
    float2 f00 = srcMinmaxMap.Load(int3( int2( 2 * threadId.x + 0, 2 * threadId.y + 0 ), currentLevel )).rg;
    float2 f10 = srcMinmaxMap.Load(int3( int2( 2 * threadId.x + 1, 2 * threadId.y + 0 ), currentLevel )).rg;
    float2 f01 = srcMinmaxMap.Load(int3( int2( 2 * threadId.x + 0, 2 * threadId.y + 1 ), currentLevel )).rg;
    float2 f11 = srcMinmaxMap.Load(int3( int2( 2 * threadId.x + 1, 2 * threadId.y + 1 ), currentLevel )).rg;

    float minH = min( min(f00.x, f10.x), min(f01.x, f11.x) ); // minimum of the minima
    float maxH = max( max(f00.y, f10.y), max(f01.y, f11.y) ); // maximum of the maxima

    dstMinmaxMap[threadId.xy] = float2( minH, maxH );
}
