#pragma once

#include "Main.h"

enum FontType
{
    FONT_NORMAL,
    FONT_LARGE
};


class Font
{
public:
    static void Init();
    static void DrawText(const FontType font, const std::string message, uint32_t color, int x, int y);
};