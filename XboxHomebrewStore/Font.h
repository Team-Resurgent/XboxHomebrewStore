#pragma once

#include "Main.h"

enum FontType
{
    FONT_NORMAL,
    FONT_LARGE
};

typedef struct
{
    float    offset;
    uint32_t lastTick;
    float    pauseTimer;
    float    textWidth;
    bool     active;
} ScrollState;

class Font
{
public:
    static void Init();
    static void MeasureText(const FontType font, const std::string message, float* outWidth);
    static void MeasureTextWrapped(const FontType font, const std::string message, float maxWidth, float* outWidth, float* outHeight);
    static std::string TruncateText(const FontType font, const std::string message, float maxWidth);
    static void DrawText(const FontType font, const std::string message, uint32_t color, float x, float y);
    static void DrawTextWrapped(const FontType font, const std::string message, uint32_t color, float x, float y, float maxWidth);
    static void DrawTextScrolling(const FontType font, const std::string message, uint32_t color, float x, float y, float maxWidth, ScrollState& scrollState);
};