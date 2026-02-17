#include "Drawing.h"

#define SSFN_IMPLEMENTATION
#define SFFN_MAXLINES 8192
#define SSFN_memcmp memcmp
#define SSFN_memset memset
#define SSFN_realloc realloc
#define SSFN_free free
#include "ssfn.h"
#include "freesans_sfn.h"

#define DRAW_BATCH_MAX_VERTS 16380

namespace 
{
    D3DDevice* mD3dDevice;
}

void Drawing::Init(D3DDevice* d3dDevice)
{
    mD3dDevice = d3dDevice;
}

void Drawing::Swizzle(const void* src, const uint32_t& depth, const uint32_t& width, const uint32_t& height, void* dest) 
{
    for (UINT y = 0; y < height; y++) {
        UINT sy = 0;
        if (y < width) {
            for (int bit = 0; bit < 16; bit++)
                sy |= ((y >> bit) & 1) << (2 * bit);
            sy <<= 1; // y counts twice
        } else {
            UINT y_mask = y % width;
            for (int bit = 0; bit < 16; bit++)
                sy |= ((y_mask >> bit) & 1) << (2 * bit);
            sy <<= 1; // y counts twice
            sy += (y / width) * width * width;
        }
        BYTE* s = (BYTE*)src + y * width * depth;
        for (UINT x = 0; x < width; x++) {
            UINT sx = 0;
            if (x < height * 2) {
                for (int bit = 0; bit < 16; bit++)
                    sx |= ((x >> bit) & 1) << (2 * bit);
            } else {
                int x_mask = x % (2 * height);
                for (int bit = 0; bit < 16; bit++)
                    sx |= ((x_mask >> bit) & 1) << (2 * bit);
                sx += (x / (2 * height)) * 2 * height * height;
            }
            BYTE* d = (BYTE*)dest + (sx + sy) * depth;
            for (unsigned int i = 0; i < depth; ++i)
                *d++ = *s++;
        }
    }
}

bool Drawing::TryCreateImage(uint8_t* imageData, D3DFORMAT format, int width, int height, Image* image) 
{
    image->width = width;
    image->height = height;

    if (FAILED(D3DXCreateTexture(mD3dDevice, image->width, image->height, 1, 0, format, D3DPOOL_DEFAULT, &image->texture))) {
        return false;
    }

    D3DSURFACE_DESC surfaceDesc;
    image->texture->GetLevelDesc(0, &surfaceDesc);
    image->uv_width = image->width / (float)surfaceDesc.Width;
    image->uv_height = image->height / (float)surfaceDesc.Height;
    
    D3DLOCKED_RECT lockedRect;
    if (SUCCEEDED(image->texture->LockRect(0, &lockedRect, NULL, 0))) {
        uint8_t* tempBuffer = (uint8_t*)malloc(surfaceDesc.Size);
        memset(tempBuffer, 0, surfaceDesc.Size);
        uint8_t* src = imageData;
        uint8_t* dst = tempBuffer;
        for (int32_t y = 0; y < image->height; y++) {
            memcpy(dst, src, image->width * 4);
            src += image->width * 4;
            dst += surfaceDesc.Width * 4;
        }
        Swizzle(tempBuffer, 4, surfaceDesc.Width, surfaceDesc.Height, lockedRect.pBits);
        free(tempBuffer);
        image->texture->UnlockRect(0);
    }

    return true;
}

bool Drawing::LoadFont(const std::string filePath, void* context)
{
    FILE* fp = fopen(filePath.c_str(), "rb");
    if (!fp) {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { 
        fclose(fp); 
        return false; 
    }

    int32_t result = -1;
    uint8_t* fontData = (uint8_t*)malloc(size);
    if (fread(fontData, 1, (size_t)size, fp) == (size_t)size) {
        memset((ssfn_t*)context, 0, sizeof(ssfn_t));
        result = ssfn_load((ssfn_t*)context, fontData);
    } else {
        free(fontData);
    }
    fclose(fp);
    return result == 0;
}


bool Drawing::TryGenerateBitmapFont(void* context, const std::string fontName, int fontStyle, int fontSize, int lineHeight, int spacing, int textureDimension, BitmapFont* bitmapFont) 
{
    ssfn_select((ssfn_t*)context, SSFN_FAMILY_ANY, fontName.c_str(), fontStyle, fontSize);

    int textureWidth = textureDimension;
    int textureHeight = textureDimension;

    uint32_t* imageData = (uint32_t*)malloc(textureWidth * textureHeight * 4);
    memset(imageData, 0, textureWidth * textureHeight * 4);

    int x = 2;
    int y = 2;

    char* charsToEncode = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    char* currentCharPos = charsToEncode;
    while (*currentCharPos) {
        char* nextCharPos = currentCharPos;
        uint32_t unicode = ssfn_utf8(&nextCharPos);

        int32_t length = nextCharPos - currentCharPos;
        char* currentChar = (char*)malloc(length + 1);
        memcpy(currentChar, currentCharPos, length);
        currentChar[length] = 0;

        currentCharPos = nextCharPos;

        int bounds_width;
        int bounds_height;
        int bounds_left;
        int bounds_top;
        int ret = ssfn_bbox((ssfn_t*)context, currentChar, &bounds_width, &bounds_height, &bounds_left, &bounds_top);
        if (ret != 0) {
            free(currentChar);
            continue;
        }

        if ((x + bounds_width + 2) > textureWidth) {
            x = 2;
            y = y + bounds_height + 2;
        }

        Rect rect;
        rect.x = x;
        rect.y = y;
        rect.width = bounds_width;
        rect.height = bounds_height;

        bitmapFont->charmap[unicode] = rect;

        ssfn_buf_t buffer;
        memset(&buffer, 0, sizeof(buffer));
        buffer.ptr = (uint8_t*)imageData;
        buffer.x = x + bounds_left;
        buffer.y = y + bounds_top;
        buffer.w = textureWidth;
        buffer.h = textureHeight;
        buffer.p = textureWidth * 4;
        buffer.bg = 0xffffffff;
        buffer.fg = 0xffffffff;

        ssfn_render((ssfn_t*)context, &buffer, currentChar);

        x = x + bounds_width + 2;
        free(currentChar);
    }

    bitmapFont->line_height = lineHeight;
    bitmapFont->spacing = spacing;

    bool result = TryCreateImage((uint8_t*)imageData, D3DFMT_A8R8G8B8, textureWidth, textureHeight, &bitmapFont->image);
    free(imageData);
    return result;
}

void Drawing::DrawFont(BitmapFont* font, const char* message, uint32_t color, int x, int y)
{
    std::vector<TEXVERTEX> vertices;

    int xPos = x;
    int yPos = y;

    char* currentCharPos = (char*)message;
    while (*currentCharPos) 
    {
        char* nextCharPos = currentCharPos;
        uint32_t unicode = ssfn_utf8(&nextCharPos);

        int32_t length = nextCharPos - currentCharPos;
        char* currentChar = (char*)malloc(length + 1);
        memcpy(currentChar, currentCharPos, length);
        currentChar[length] = 0;

        currentCharPos = nextCharPos;

        Rect rect = font->charmap[unicode];
        //if doesnt exist skip

        float uvX = rect.x / (float)font->image.width;
        float uvY = rect.y / (float)font->image.height;
        float uvWidth = rect.width / (float)font->image.width;
        float uvHeight = rect.height / (float)font->image.height;

        uint32_t charColor = color;
        //if (unicode == 161) {
        //    charColor = theme::getJoyButtonAColor();
        //} else if (unicode == 162) {
        //    charColor = theme::getJoyButtonBColor();
        //} else if (unicode == 163) {
        //    charColor = theme::getJoyButtonXColor();
        //} else if (unicode == 164) {
        //    charColor = theme::getJoyButtonYColor();
        //} else if (unicode == 182) {
        //    charColor = theme::getJoyButtonBlackColor();
        //} else if (unicode == 181) {
        //    charColor = theme::getJoyButtonWhiteColor();
        //}

  
        float u0 = uvX;
        float v0 = uvY;
        float u1 = uvX + uvWidth;
        float v1 = uvY + uvHeight;

        float px = xPos - 0.5f;
        float py = yPos - 0.5f;
        float pz = 0.0f;
        float fw = font->image.width * uvWidth;
        float fh = font->image.height *uvHeight;

        TEXVERTEX tv0;
        tv0.x = px + fw;
        tv0.y = py + fh;
        tv0.z = pz;
        tv0.rhw = 1.0f;
        tv0.diffuse = color;
        tv0.u = u1;
        tv0.v = v0;
        vertices.push_back(tv0);

        TEXVERTEX tv1;
        tv1.x = px + fw;
        tv1.y = py;
        tv1.z = pz;
        tv1.rhw = 1.0f;
        tv1.diffuse = color;
        tv1.u = u1;
        tv1.v = v1;
        vertices.push_back(tv1);

        TEXVERTEX tv2;
        tv2.x = px;
        tv2.y = py;
        tv2.z = pz;
        tv2.rhw = 1.0f;
        tv2.diffuse = color;
        tv2.u = u0;
        tv2.v = v1;
        vertices.push_back(tv2);

        TEXVERTEX tv3;
        tv3.x = px + fw;
        tv3.y = py + fh;
        tv3.z = pz;
        tv3.rhw = 1.0f;
        tv3.diffuse = color;
        tv3.u = u1;
        tv3.v = v0;
        vertices.push_back(tv3);

        TEXVERTEX tv4;
        tv4.x = px;
        tv4.y = py;
        tv4.z = pz;
        tv4.rhw = 1.0f;
        tv4.diffuse = color;
        tv4.u = u0;
        tv4.v = v1;
        vertices.push_back(tv4);

        TEXVERTEX tv5;
        tv5.x = px;
        tv5.y = py + fh;
        tv5.z = pz;
        tv5.rhw = 1.0f;
        tv5.diffuse = color;
        tv5.u = u0;
        tv5.v = v0;
        vertices.push_back(tv5);

        xPos = xPos + rect.width + font->spacing;
        free(currentChar);
    }

    if (vertices.size() == 0)
    {
        return;
    }

    mD3dDevice->BeginScene();
    mD3dDevice->Clear(0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0xff000000, 1.0f, 0L);

    mD3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1 );
    mD3dDevice->SetTexture(0, font->image.texture);
    for (uint32_t offset = 0; offset < vertices.size(); )
    {
        int batchVerts = vertices.size() - offset;
        if (batchVerts > DRAW_BATCH_MAX_VERTS)
            batchVerts = DRAW_BATCH_MAX_VERTS;
        mD3dDevice->DrawPrimitiveUP(
            D3DPT_TRIANGLELIST,
            batchVerts / 3,
            &vertices[offset],
            sizeof(TEXVERTEX));
        offset += batchVerts;
    }

    mD3dDevice->EndScene();
	mD3dDevice->Present(NULL, NULL, NULL, NULL);
}