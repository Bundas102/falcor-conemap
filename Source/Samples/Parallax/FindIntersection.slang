#ifndef FINDINTERSECTION_INCLUDED
#define FINDINTERSECTION_INCLUDED
#include "Parallax.ps.slang" 

HMapIntersection findIntersection_bumpMapping(float2 u, float2 u2)
{
    HMapIntersection ret = INIT_INTERSECTION;
    ret.uv = u2;
    ret.t = 1;
    ret.last_t = 1;
    return ret;
}

HMapIntersection findIntersection_parallaxMapping(float2 u, float2 u2)
{
    float t = 1 - getH(u2);
    HMapIntersection ret = INIT_INTERSECTION;
    ret.uv = (1 - t) * u + t * u2;
    ret.t = t;
    ret.last_t = t;
    return ret;
}

HMapIntersection findIntersection_linearSearch(float2 u, float2 u2)
{
    HMapIntersection ret = INIT_INTERSECTION;
    float dt = oneOverSteps;
    float t = 1;
    float2 du = (u2 - u) * dt;
    float2 uu = u;
    for (uint i = 0; i < steps; ++i)
    {
        t -= dt;
        uu += du;
        float h = getH(uu);
        if (h >= t)
        {
            ret.wasHit = true;
            break;
        }
    }

    ret.uv = uu;
    ret.t = 1 - t;
    ret.last_t = 1 - t - dt;
    return ret;
}


// Adapted from "Cone Step Mapping: An Iterative Ray-Heightfield Intersection Algorithm" - Jonathan Dummer
HMapIntersection findIntersection_coneStepMapping(float2 u, float2 u2)
{
    float3 ds = float3(u2 - u, 1);
    ds = normalize(ds);
    float w = 1 / HMres.x;
    float iz = sqrt(1.0 - ds.z * ds.z); // = length(ds.xy)
    float sc = 0;
    float2 t = getHC_texture(u);
    int stepCount = 0;
    float zTimesSc = 0.0;
    while (1.0 - ds.z * sc > t.x && stepCount < steps)
    {
        zTimesSc = ds.z * sc;
        sc += relax * (w + (1.0 - zTimesSc - t.x) / (ds.z + iz / ( /*t.y **/t.y)));
        t = getHC_texture(u + ds.xy * sc);
        ++stepCount;
    }
    
    HMapIntersection ret = INIT_INTERSECTION;
    ret.last_t = zTimesSc;
    ret.wasHit = (stepCount < steps);
    sc -= w;
    float tt = ds.z * sc;
    ret.uv = (1 - tt) * u + tt * u2;
    ret.t = tt;
    return ret;
}


// u: frontPlate tex coords, u2 back plate tex coords
// scale: uniform scale of 
HMapIntersection findIntersection(float2 u, float2 u2)
{
#ifndef PARALLAX_FUN
    errorf("PARALLAX_FUN is not defined");
    HMapIntersection r;
    return r;
#elif PARALLAX_FUN == 0
    return findIntersection_bumpMapping(u, u2);
#elif PARALLAX_FUN == 1
    return findIntersection_parallaxMapping(u, u2);
#elif PARALLAX_FUN == 2
    return findIntersection_linearSearch(u, u2);
#elif PARALLAX_FUN == 3
    return findIntersection_coneStepMapping(u, u2);
#else
    errorf("PARALLAX_FUN has an unused value: %w", PARALLAX_FUN);
    HMapIntersection r; return r;
#endif
}
#endif
