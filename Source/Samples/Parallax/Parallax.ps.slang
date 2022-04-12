#ifndef PARALLAX_PS_INCLUDED 
#define PARALLAX_PS_INCLUDED 

cbuffer FScb : register(b0)
{
    float3 lightDir;
    float3 camPos;
    float2 center;
    float2 HMres;
    float2 HMres_r;
    float  scale;
    float  heightMapHeight;
    float  relax;
    uint   steps;
    uint   refine_steps;
    float  oneOverSteps;
    float  lightIntensity;
    bool   discardFragments;
    bool   displayNonConverged;
    int    const_isolate;
};

Texture2D gTexture;
Texture2D gAlbedoTexture;
SamplerState gSampler;

struct FsIn
{
    float4 posH : SV_POSITION;
    float2 texC : TEXCOORD;
    float3 posW : POSITION;
    float3 norm : NORMAL;
};


float getH_texture(float2 uv)
{
    return gTexture.Sample(gSampler, uv).r;
}


float getH(float2 uv)
{
    return getH_texture(uv);
}
float2 getHC_texture(float2 uv)
{
    return gTexture.Sample(gSampler, uv).rg;
}

float3 getNormalTBN_finiteDiff(float2 uv)
{
    const float mutliplier = 1;
    float2 delta = HMres_r * mutliplier;
    float2 one_over_delta = HMres / mutliplier;
    
    float2 du = float2(delta.x, 0);
    float2 dv = float2(0, delta.y);
    float dhdu = 0.5 * one_over_delta.x * (getH(uv + du) - getH(uv - du));
    float dhdv = 0.5 * one_over_delta.y * (getH(uv + dv) - getH(uv - dv));
    return normalize(float3(-dhdu, -dhdv, 1));
}

float3 getNormalTBN(float2 uv)
{
    return getNormalTBN_finiteDiff(uv);
}


struct HMapIntersection
{
    float2 uv;
    float t;    // uv = (1-t)*u + t*u2
    float last_t;
    bool wasHit;
};

static const HMapIntersection INIT_INTERSECTION = { 0.0.xx, 0.0, 0.0, false };

#include "FindIntersection.slang"
#include "Refinement.slang"

// invert a 3x3 matrix
// input: the 3 columns of the matrix
float3x3 invMat(float3 a, float3 b, float3 c)
{
    float3 r1 = cross(b, c);
    float3 r2 = cross(c, a);
    float3 r3 = cross(a, b);
    float invDet = 1 / dot(r3, c);
    return invDet * float3x3(r1, r2, r3);
}

float4 main(FsIn fs) : SV_TARGET
{
    float2 u = fs.texC; // original texcoords
    float3 p = fs.posW; // fragment world pos
    float3 col = float3(.5);

    // calc tangent space
    float2 dxu = ddx_fine(u);
    float2 dyu = ddy_fine(u);
    float3 dxp = ddx_fine(p);
    float3 dyp = ddy_fine(p);
    
    float2x3 P = float2x3(dxp, dyp);
    float2x2 U = float2x2(dxu, dyu);
    float2x2 U_inv = float2x2(U[1][1], -U[0][1], -U[1][0], U[0][0]) / determinant(U);
    float2x3 A = mul(U_inv, P);
    float3 T = A[0];
    float3 B = A[1];
    float3 N = normalize(fs.norm) * heightMapHeight;

    float3x3 TBN = float3x3(T, B, N);
    float3x3 TBN_inv = transpose(invMat(T, B, N));

    float3 viewDirW = p - camPos;
    float3 viewDirT = mul(viewDirW, TBN_inv);

    // texcoords where the camera ray intersects the bottom plate
    float2 u2 = u - viewDirT.xy / viewDirT.z;

    // find the intersection with the height map
    HMapIntersection I = findIntersection(u, u2);
    float2 u3 = refineIntersection(I, u, u2);
    
    // intersection is outside of the bottom plate
    if (any(u3 < 0) || any(u3 > 1))
    {
        if (discardFragments)
            discard;
        return float4(1, 0, 0, 1);
    }
    
    // get normal in TBN space
    float3 normT = getNormalTBN(u3);
    // transform normal from TBN to world coordinates
    float3 norm = normalize(mul(normT, transpose(TBN_inv)));
    

    // fetch the final albedo color
#if defined( USE_ALBEDO_TEXTURE ) && USE_ALBEDO_TEXTURE
    float3 albedo = gAlbedoTexture.Sample(gSampler, u3).rgb;
    col *= albedo;
#endif
    

    
    float diffuse = lightIntensity * saturate(dot(-normalize(lightDir), norm));
    col.rgb *= diffuse;

    if (displayNonConverged && !I.wasHit)
    {
        col.rgb = float3(1,0,1);
    }
    
    return float4(col, 1);
}
#endif
