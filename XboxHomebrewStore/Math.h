#pragma once

#include "Main.h"

class Math
{
public:
    static int32_t MinInt32(int32_t value, int32_t min);
    static int32_t MaxInt32(int32_t value, int32_t max);
    static int32_t ClampInt32(int32_t value, int32_t min, int32_t max);
    static float ClampFloat(float value, float min, float max);
    static float CopySign(float a, float b);
    static int32_t AspectScaleWidth(int32_t originalWidth, int32_t originalHeight, int32_t targetHeight);
};
