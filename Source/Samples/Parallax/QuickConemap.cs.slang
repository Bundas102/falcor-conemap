cbuffer CScb : register(b0)
{
    uint maxLevel; // ending mipmap level
    uint2 maxSize; // texture size
    float2 deltaHalf; // (UV size of a texel)/2
};

Texture2D<float2> srcMinmaxMap; // float2 valued, storing [min, max]
RWTexture2D<float2> dstConeMap; // float2 valued, storing [height, cone tan]

// Get texture coodinate of texel center from texel index
float2 texCoord(uint2 texelInd, uint2 textureSize)
{
    return ((float2) texelInd + 0.5) / textureSize;
}


void checkNeighbour(bool cond, float dist, uint2 IJ, uint level, float baseH, inout float minTan)
{
    if (!cond) return;
    float nMaxHeight = srcMinmaxMap.Load(int3(IJ, level)).g; // .g : max
    float heightDiff = nMaxHeight - baseH;
    if (heightDiff > dist)
    {
        minTan = min(minTan, dist / heightDiff );
    }
}

// This function creates a conemap from a minmax-mipmap of a heightmap
void generateQuickConeMap_naive(uint3 threadId)
{
    if (any(threadId.xy >= maxSize)) return;

    float baseH = srcMinmaxMap.Load(int3(threadId.xy, 0)).g;
    float minTan = 1;
    uint2 currIJ = threadId.xy;       // texel index on the current level
    uint2 currSize = maxSize;         // size of the current level
    float2 currDeltaHalf = deltaHalf; // (distance between neighbouring texels)/2
    const float2 baseUV = texCoord(currIJ, currSize);
    float2 currUV = baseUV;
    float2 currRelative = float2(0, 0);
    float4 distLeftTopRightBottom = 2 * float4(deltaHalf, deltaHalf);
    for (uint currLevel = 0; currLevel <= maxLevel; ++currLevel)
    {
        int2 IJ;
        // orthogonal neighbours
        {
            // left
            IJ = currIJ + int2(-1, 0);
            checkNeighbour(IJ.x >= 0, distLeftTopRightBottom.x, IJ, currLevel, baseH, minTan);
            // right
            IJ = currIJ + int2(+1, 0);
            checkNeighbour(IJ.x < currSize.x, distLeftTopRightBottom.z, IJ, currLevel, baseH, minTan);
            // top
            IJ = currIJ + int2(0, -1);
            checkNeighbour(IJ.y >= 0, distLeftTopRightBottom.y, IJ, currLevel, baseH, minTan);
            // bottom
            IJ = currIJ + int2(0, +1);
            checkNeighbour(IJ.y < currSize.y, distLeftTopRightBottom.w, IJ, currLevel, baseH, minTan);
        }
        // diagonal neighbours
        {
            // top-left
            IJ = currIJ + int2(-1, -1);
            checkNeighbour(IJ.x >= 0 && IJ.y >= 0, length(distLeftTopRightBottom.xy), IJ, currLevel, baseH, minTan);
            // top-right
            IJ = currIJ + int2(+1, -1);
            checkNeighbour(IJ.x < currSize.x && IJ.y >= 0, length(distLeftTopRightBottom.zy), IJ, currLevel, baseH, minTan);
            // bottom-right
            IJ = currIJ + int2(+1, +1);
            checkNeighbour(IJ.x < currSize.x && IJ.y < currSize.y, length(distLeftTopRightBottom.zw), IJ, currLevel, baseH, minTan);
            // bottom-left
            IJ = currIJ + int2(-1, +1);
            checkNeighbour(IJ.x >= 0 && IJ.y < currSize.y, length(distLeftTopRightBottom.xw), IJ, currLevel, baseH, minTan);
        }
        currIJ /= 2;
        currSize /= 2;
        currDeltaHalf *= 2;
        currUV = texCoord(currIJ, currSize);
        currRelative = baseUV - currUV;
#if MAX_AT_TEXEL_CENTER == 1
        distLeftTopRightBottom = float4(
            currDeltaHalf + currRelative + currDeltaHalf,
            currDeltaHalf - currRelative + currDeltaHalf);
#else
        distLeftTopRightBottom = float4(
            currDeltaHalf + currRelative + deltaHalf,
            currDeltaHalf - currRelative + deltaHalf);
#endif
    }
    dstConeMap[threadId.xy] = float2(baseH, minTan);
}

float calcNearestUncheckedDiagonalDist(bool2 isSpec, float2 distXY, bool x_closer_than_y, float2 delta)
{
    float2 s = distXY - delta * isSpec;
    s = all(isSpec) ? x_closer_than_y ? float2(distXY.x, s.y) : float2(s.x, distXY.y) : s;
    return length(s);
}

// This function creates a conemap from a minmax-mipmap of a heightmap
// improved version using region growing
void generateQuickConeMap_regionGrowing3x3(uint3 threadId)
{
    if (any(threadId.xy >= maxSize))
        return;

    float baseH = srcMinmaxMap.Load(int3(threadId.xy, 0)).g;
    float minTan = 1;
    uint2 currIJ = threadId.xy; // texel index on the current level
    uint2 currSize = maxSize; // size of the current level
    float2 currDeltaHalf = deltaHalf; // (distance between neighbouring texels)/2
    const float2 baseUV = texCoord(currIJ, currSize);
    float2 currUV = baseUV;
    float2 currRelative = float2(0, 0);
    float4 distLeftTopRightBottom = 2 * float4(deltaHalf, deltaHalf);

    // first step is the 8 neighbour
    {
        int2 IJ;
        // orthogonal neighbours
        {
            // left
            IJ = currIJ + int2(-1, 0);
            checkNeighbour(IJ.x >= 0, distLeftTopRightBottom.x, IJ, 0, baseH, minTan);
            // right
            IJ = currIJ + int2(+1, 0);
            checkNeighbour(IJ.x < currSize.x, distLeftTopRightBottom.z, IJ, 0, baseH, minTan);
            // top
            IJ = currIJ + int2(0, -1);
            checkNeighbour(IJ.y >= 0, distLeftTopRightBottom.y, IJ, 0, baseH, minTan);
            // bottom
            IJ = currIJ + int2(0, +1);
            checkNeighbour(IJ.y < currSize.y, distLeftTopRightBottom.w, IJ, 0, baseH, minTan);
        }
        // diagonal neighbours
        {
            // top-left
            IJ = currIJ + int2(-1, -1);
            checkNeighbour(IJ.x >= 0 && IJ.y >= 0, length(distLeftTopRightBottom.xy), IJ, 0, baseH, minTan);
            // top-right
            IJ = currIJ + int2(+1, -1);
            checkNeighbour(IJ.x < currSize.x && IJ.y >= 0, length(distLeftTopRightBottom.zy), IJ, 0, baseH, minTan);
            // bottom-right
            IJ = currIJ + int2(+1, +1);
            checkNeighbour(IJ.x < currSize.x && IJ.y < currSize.y, length(distLeftTopRightBottom.zw), IJ, 0, baseH, minTan);
            // bottom-left
            IJ = currIJ + int2(-1, +1);
            checkNeighbour(IJ.x >= 0 && IJ.y < currSize.y, length(distLeftTopRightBottom.xw), IJ, 0, baseH, minTan);
        }
    }
    
    for (uint currLevel = 1; currLevel <= maxLevel; ++currLevel)
    {
        uint2 lastIJ = currIJ;
        float2 lastUV = currUV;
        float2 lastRelative = currRelative;
        
        currIJ /= 2;
        currSize /= 2;
        currUV = texCoord(currIJ, currSize);
        currRelative = baseUV - currUV;
        
#if MAX_AT_TEXEL_CENTER == 1
        distLeftTopRightBottom = float4(
            4 * currDeltaHalf + currRelative,
            4 * currDeltaHalf - currRelative);
#else
        distLeftTopRightBottom = float4(
            3 * currDeltaHalf + lastRelative + deltaHalf,
            3 * currDeltaHalf - lastRelative + deltaHalf);
#endif
        currDeltaHalf *= 2;
        
        int2 IJ;
        // orthogonal neighbours
        {
            // left
            IJ = currIJ + int2(-1, 0);
            checkNeighbour(IJ.x >= 0, distLeftTopRightBottom.x, IJ, currLevel, baseH, minTan);
            // right
            IJ = currIJ + int2(+1, 0);
            checkNeighbour(IJ.x < currSize.x, distLeftTopRightBottom.z, IJ, currLevel, baseH, minTan);
            // top
            IJ = currIJ + int2(0, -1);
            checkNeighbour(IJ.y >= 0, distLeftTopRightBottom.y, IJ, currLevel, baseH, minTan);
            // bottom
            IJ = currIJ + int2(0, +1);
            checkNeighbour(IJ.y < currSize.y, distLeftTopRightBottom.w, IJ, currLevel, baseH, minTan);
        }
        // diagonal neighbours
        {
            float dist;
            // top-left
            IJ = currIJ + uint2(-1, -1);
#if MAX_AT_TEXEL_CENTER == 1
            dist = length(distLeftTopRightBottom.xy);
#else
            dist = calcNearestUncheckedDiagonalDist(
                lastIJ % 2 == uint2(0, 0),
                distLeftTopRightBottom.xy,
                lastRelative.x > lastRelative.y, currDeltaHalf);
#endif
            checkNeighbour(IJ.x >= 0 && IJ.y >= 0, dist, IJ, currLevel, baseH, minTan);
            // top-right
            IJ = currIJ + uint2(+1, -1);
#if MAX_AT_TEXEL_CENTER == 1
            dist = length(distLeftTopRightBottom.zy);
#else
            dist = calcNearestUncheckedDiagonalDist(
                lastIJ % 2 == uint2(1, 0),
                distLeftTopRightBottom.zy,
                lastRelative.x + lastRelative.y > 0, currDeltaHalf);
#endif
            checkNeighbour(IJ.x < currSize.x && IJ.y >= 0, dist, IJ, currLevel, baseH, minTan);
            // bottom-right
            IJ = currIJ + uint2(+1, +1);
#if MAX_AT_TEXEL_CENTER == 1
            dist = length(distLeftTopRightBottom.zw);
#else
            dist = calcNearestUncheckedDiagonalDist(
                lastIJ % 2 == uint2(1, 1),
                distLeftTopRightBottom.zw,
                lastRelative.x > lastRelative.y > 0, currDeltaHalf);
#endif
            checkNeighbour(IJ.x < currSize.x && IJ.y < currSize.y, dist, IJ, currLevel, baseH, minTan);
            // bottom-left
            IJ = currIJ + uint2(-1, +1);
#if MAX_AT_TEXEL_CENTER == 1
            dist = length(distLeftTopRightBottom.xw);
#else
            dist = calcNearestUncheckedDiagonalDist(
                lastIJ % 2 == uint2(-1, 1),
                distLeftTopRightBottom.xw,
                lastRelative.x + lastRelative.y < 0, currDeltaHalf);
#endif
            checkNeighbour(IJ.x >= 0 && IJ.y < currSize.y, dist, IJ, currLevel, baseH, minTan);
        }
    }
    dstConeMap[threadId.xy] = float2(baseH, minTan);
}


[numthreads(16,16,1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
#ifndef QUICK_GEN_ALG
    errorf("QUICK_GEN_ALG is undefined");
#elif QUICK_GEN_ALG == 1
    generateQuickConeMap_naive(threadId);
#elif QUICK_GEN_ALG == 2
    generateQuickConeMap_regionGrowing3x3(threadId);
#else
    errorf("Unknown QUICK_GEN_ALG: %w", QUICK_GEN_ALG);
#endif
}

