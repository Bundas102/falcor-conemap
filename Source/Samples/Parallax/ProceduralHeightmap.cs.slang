cbuffer CScb : register(b0)
{
    uint2 maxSize;
    int4 intParams;
    float4 floatParams;
};



float paramFun_sinc(float2 t)
{
    // domain
    t = 10 * t - 5;
    // sinc
    float len2 = dot(t, t);
    return 0.8 * sin(len2) / len2 + 0.2;
}
float paramFun_spheres(float2 t)
{
    float r = floatParams.x;
    float n = float(intParams.x);
    // domain
    t = (t * n) % 1.0;
    t = 2.0 * t - 1.0;
    // sphere
    float len2 = dot(t, t);
    return 1 - r + ((len2 >= r * r) ? 0 : sqrt(r * r - len2));
}

float getH(float2 t)
{
#ifndef HEIGHT_FUN
    return paramFun_sinc(t);
#elif HEIGHT_FUN == 0
    return paramFun_sinc(t);
#elif HEIGHT_FUN == 1
    return paramFun_spheres(t);
#else
    return paramFun_sinc(t);
#endif
}

RWTexture2D<float> tex2D_uav;

[numthreads(16, 16, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    if (any(threadId.xy >= maxSize))
        return;
    float2 t = ((float2) threadId.xy + 0.5) / maxSize; // texture coords
    float val = getH(t);
    tex2D_uav[threadId.xy].x = val;
}
