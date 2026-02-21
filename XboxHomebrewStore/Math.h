#pragma once

#include "Main.h"

class Math
{
public:
    static int32_t MinInt32(int32_t value, int32_t min);
    static int32_t MaxInt32(int32_t value, int32_t max);
    static uint32_t MinUint32(uint32_t value, uint32_t min);
    static uint32_t MaxUint32(uint32_t value, uint32_t max);
    static int32_t ClampInt32(int32_t value, int32_t min, int32_t max);
    static float ClampFloat(float value, float min, float max);
    static float CopySign(float a, float b);
};
