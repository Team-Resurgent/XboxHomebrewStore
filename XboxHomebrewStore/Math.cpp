#include "Math.h"

int32_t Math::ClampInt32(int32_t value, int32_t min, int32_t max)
{
    return (value < min) ? min : (value > max) ? max : value;
}
