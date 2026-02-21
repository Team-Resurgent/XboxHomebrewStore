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
    int      textWidth;
    bool     active;
} ScrollState;

class Font
{
public:
    static void Init();
    static int MeasureText(const FontType font, const std::string& message);
    static std::string TruncateText(const FontType font, const std::string& message, int maxWidth);
    static void DrawText(const FontType font, const std::string message, uint32_t color, int x, int y);
    static void DrawTextScrolling(const FontType font, const std::string& message, uint32_t color, int x, int y, int maxWidth, ScrollState* scrollState);
};