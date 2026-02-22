#include "Math.h"

int32_t Math::MinInt32(int32_t value, int32_t min)
{
    return (value < min) ? value : min;
}

int32_t Math::MaxInt32(int32_t value, int32_t max)
{
    return (value > max) ? value : max;
}

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

int32_t Math::AspectScaleWidth(int32_t originalWidth, int32_t originalHeight, int32_t targetHeight)
{
    if (originalHeight <= 0) {
        return originalWidth;
    }
    return (int32_t)(originalWidth * ((float)targetHeight / (float)originalHeight));
}