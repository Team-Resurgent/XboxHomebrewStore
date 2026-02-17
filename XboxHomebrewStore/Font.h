#pragma once

#include "Main.h"

class Font
{
public:
    static void Init(D3DDevice* d3dDevice);
    static void DrawText(const char* message, uint32_t color, int x, int y);
};