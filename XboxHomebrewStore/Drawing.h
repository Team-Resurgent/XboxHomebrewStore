#pragma once

#include "Main.h"

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} Rect;

typedef struct {
    int32_t width;
    int32_t height;
    float uv_width;
    float uv_height;
    D3DTexture* texture;
} Image;

typedef struct 
{    
    std::map<uint32_t, Rect> charmap;
    int32_t line_height;
    int32_t spacing;
    Image image;
} BitmapFont;

typedef struct
{
    float x, y, z, rhw;
    uint32_t diffuse;
    float u, v;
} TEXVERTEX;

typedef struct
{
    float x, y, z, rhw;
    uint32_t diffuse;
} VERTEX;

class Drawing
{
public:
    static void SaveRenderState();
    static void RestoreRenderState();
    static void Init();
    static void Swizzle(const void* src, const uint32_t& depth, const uint32_t& width, const uint32_t& height, void* dest);
    static bool TryCreateImage(uint8_t* imageData, D3DFORMAT format, int32_t width, int32_t height, Image* image);
    static bool LoadFont(const std::string filePath, void* context);
    static bool TryGenerateBitmapFont(void* context, const std::string fontName, int32_t fontStyle, int32_t fontSize, int32_t lineHeight, int32_t spacing, int32_t textureDimension, BitmapFont* bitmapFont);
    static void DrawFont(BitmapFont* font, const std::string message, uint32_t color, float x, float y);
    static void DrawFontWrapped(BitmapFont* font, const std::string message, uint32_t color, float x, float y, float maxWidth);
    static void DrawFilledRect(uint32_t color, float x, float y, float width, float height);
    static void DrawTexturedRect(D3DTexture* texture, uint32_t diffuse, float x, float y, float width, float height);
    static void DrawNinePatch(D3DTexture* texture, uint32_t diffuse, float x, float y, float width, float height, float cornerWidthPx = 0, float cornerHeightPx = 0, float contentWidthPx = 0, float contentHeightPx = 0);
    static void BeginStencil(float x, float y, float w, float h);
    static void EndStencil();
};