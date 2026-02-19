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
    static void Init(D3DDevice* d3dDevice);
    static void DrawText(const FontType font, const char* message, uint32_t color, int x, int y);
};