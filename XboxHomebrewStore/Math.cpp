#include "Math.h"

int32_t Math::ClampInt32(int32_t value, int32_t min, int32_t max)
{
    return (value < min) ? min : (value > max) ? max : value;
}


float Math::ClampFloat(float value, float min, float max)
{
    return (value < min) ? min : (value > max) ? max : value;
}

float Math::CopySign(float a, float b)
{
    return (b >= 0.0f) ? fabsf(a) : -fabsf(a);
}