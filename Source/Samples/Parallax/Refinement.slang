#ifndef REFINEMENT_INCLUDED
#define REFINEMENT_INCLUDED
#include "Parallax.ps.slang"

float2 refineIntersection_linearApprox(HMapIntersection interval, float2 u0, float2 u1)
{
    float t0 = interval.last_t;
    float t1 = interval.t;
    float h0 = getH(lerp(u0, u1, t0));
    float h1 = getH(lerp(u0, u1, t1));
    float dt = t1 - t0;
    float t = (dt + t0 * h1 - t1 * h0) / (dt + h1 - h0);
    t = clamp(t, t0, t1);
    return lerp(u0, u1, t);
}

float2 refineIntersection_binarySearch(HMapIntersection interval, float2 u0, float2 u1)
{
    float t0 = interval.last_t;
    float t1 = interval.t;
    float th = 0.5 * (t0 + t1);
    for (uint i = 0; i < refine_steps; ++i)
    {
        float fh = getH(lerp(u0, u1, th));
        if (fh > 1 - th)
            t1 = th;
        else
            t0 = th;
        th = 0.5 * (t0 + t1);
    }
    
    return lerp(u0, u1, th);
}

float2 refineIntersection(HMapIntersection interval, float2 u0, float2 u1)
{
#ifndef REFINE_FUN
    errorf("REFINE_FUN is not defined");
    return interval.uv;
#elif REFINE_FUN == 0
    return interval.uv;
#elif REFINE_FUN == 1
    return refineIntersection_linearApprox(interval, u0, u1);
#elif REFINE_FUN == 2
    return refineIntersection_binarySearch(interval, u0, u1);
#else
    errorf("REFINE_FUN has an unused value: %w", REFINE_FUN);
    return interval.uv;
#endif
}
#endif
