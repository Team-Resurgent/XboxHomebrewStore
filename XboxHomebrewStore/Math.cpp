#include "Math.h"

int32_t Math::MinInt32(int32_t value, int32_t min)
{
    return (value < min) ? value : min;
}

int32_t Math::MaxInt32(int32_t value, int32_t max)
{
    return (value > max) ? value : max;
}

uint32_t Math::MinUint32(uint32_t value, uint32_t min)
{
    return (value < min) ? value : min;
}

uint32_t Math::MaxUint32(uint32_t value, uint32_t max)
{
    return (value > max) ? value : max;
}

int32_t Math::ClampInt32(int32_t value, int32_t min, int32_t max)
{
    return (value < min) ? min : (value > max) ? max : value;
}

uint32_t Math::ClampUint32(uint32_t value, uint32_t min, uint32_t max)
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