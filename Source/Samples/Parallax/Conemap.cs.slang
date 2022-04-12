cbuffer CScb : register(b0)
{
    uint2 maxSize;
    float2 oneOverMaxSize;
    uint searchSteps;
    float oneOverSearchSteps;
    int srcLevel;
};

Texture2D<float> heightMap;
RWTexture2D<float2> coneMap; // [height, cone alpha]
SamplerState gSampler : register(s0);

float getH(float2 uv)
{
    return heightMap.SampleLevel(gSampler, uv, srcLevel).r;
}

float2 texCoord(uint2 texelInd)
{
    return ((float2) texelInd + 0.5) * oneOverMaxSize;
}

float dot2(float2 a)
{
    return dot(a, a);
}

float getConservativeCone(float baseHeight, float2 baseTexCoord, uint2 texelInd)
{
    float2 t = texCoord(texelInd);
    float d2 = dot2(baseTexCoord - t);
    float deltaH = heightMap.Load(int3(texelInd, srcLevel)) - baseHeight;

    return deltaH <= 0 ? 1.0 : sqrt(d2) / deltaH;
}

// Adapted from https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-18-relaxed-cone-stepping-relief-mapping
float getRelaxedCone(float baseHeight, float2 baseTexCoord, uint2 texelInd, float minRatio)
{
    float2 t = texCoord(texelInd);
    
    float3 src = float3(baseTexCoord, 1+0.001);
    float3 dst = float3(t, heightMap.Load(int3(texelInd, srcLevel)));

    if ((dst.z <= baseHeight) || length(dst.xy - baseTexCoord) > minRatio * (dst.z - baseHeight))
        return 1;
    
    float3 vec = dst - src; // Ray direction
    vec /= -vec.z; // Scale ray direction so that vec.z = -1.0
    vec *= dst.z; // Scale again
    float3 step_fwd = vec * oneOverSearchSteps; // Length of a forward step
    // Search until a new point outside the surface
    float3 ray_pos = dst + step_fwd;
    for (uint i = 1; i < searchSteps; i++)
    {
        float current_height = getH(ray_pos.xy);
        if (current_height >= ray_pos.z)
        {
            ray_pos += step_fwd;
        }
        else
        {
            break;
        }
    }
    // Original texel depth
    float src_texel_height = baseHeight;
    // Compute the cone ratio
    float cone_ratio = (ray_pos.z <= src_texel_height) ? 1.0 :
        length(ray_pos.xy - baseTexCoord) / (ray_pos.z - src_texel_height);
    return cone_ratio;
}


float getCone(float baseHeight, float2 baseTexCoord, uint2 texelInd, float minRatio)
{
#ifndef CONE_TYPE
    errorf("CONE_TYPE is undefined");
    return 0;
#elif CONE_TYPE == 1
    return getConservativeCone(baseHeight, baseTexCoord, texelInd);
#elif CONE_TYPE == 2
    return getRelaxedCone(baseHeight, baseTexCoord, texelInd, minRatio);
#else
    errorf("Unknown CONE_TYPE: %w", CONE_TYPE);
    return 0;
#endif
}


[numthreads(16, 16, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId.xy >= maxSize))
        return;
    float2 baseT = texCoord(threadId.xy); // texture coords
    float baseH = heightMap.Load(int3(threadId.xy, srcLevel));

    //float maxCos = 0;
    float minTan = 1;
    uint w = maxSize.x;
    uint h = maxSize.y;
    for (uint i = 0; i < w; ++i)
    {
        for (uint j = 0; j < h; ++j)
        {
            uint2 id = uint2(i, j);
            if (any(threadId.xy != id))
            {
                minTan = min(minTan, getCone(baseH, baseT, id, minTan));

            }
        }
    }
    coneMap[threadId.xy] = float2(baseH, minTan);
}

