#pragma once

#include "Main.h"

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rect;

typedef struct {
    int width;
    int height;
    float uv_width;
    float uv_height;
    D3DTexture* texture;
} Image;

typedef struct 
{    
    std::map<uint32_t, Rect> charmap;
    int line_height;
    int spacing;
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
    static bool TryCreateImage(uint8_t* imageData, D3DFORMAT format, int width, int height, Image* image);
    static bool LoadFont(const std::string filePath, void* context);
    static bool TryGenerateBitmapFont(void* context, const std::string fontName, int fontStyle, int fontSize, int lineHeight, int spacing, int textureDimension, BitmapFont* bitmapFont);
    static void DrawFont(BitmapFont* font, const std::string message, uint32_t color, float x, float y);
    static void DrawFilledRect(uint32_t color, float x, float y, float width, float height);
    static void DrawTexturedRect(D3DTexture* texture, uint32_t diffuse, float x, float y, float width, float height);
    static void DrawNinePatch(D3DTexture* texture, uint32_t diffuse, float x, float y, float width, float height, float cornerWidthPx = 0, float cornerHeightPx = 0, float contentWidthPx = 0, float contentHeightPx = 0);
    static void BeginStencil(float x, float y, float w, float h);
    static void EndStencil();
};