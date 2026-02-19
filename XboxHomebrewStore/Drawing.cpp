#include "Drawing.h"

#define SSFN_IMPLEMENTATION
#define SFFN_MAXLINES 8192
#define SSFN_memcmp memcmp
#define SSFN_memset memset
#define SSFN_realloc realloc
#define SSFN_free free
#include "ssfn.h"

#define DRAW_BATCH_MAX_VERTS 16380

namespace 
{
    D3DDevice* mD3dDevice;
    LPDIRECT3DSTATEBLOCK8 mSavedStateBlock = NULL;
}

void Drawing::Init(D3DDevice* d3dDevice)
{
    mD3dDevice = d3dDevice;
}

void Drawing::SaveRenderState()
{
    if (mSavedStateBlock != NULL)
    {
        mSavedStateBlock->Release();
        mSavedStateBlock = NULL;
    }
    if (mD3dDevice != NULL)
        mD3dDevice->CreateStateBlock(D3DSBT_ALL, &mSavedStateBlock);
}

void Drawing::RestoreRenderState()
{
    if (mSavedStateBlock != NULL)
    {
        mSavedStateBlock->Apply();
        mSavedStateBlock->Release();
        mSavedStateBlock = NULL;
    }
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
    static TEXVERTEX batchBuf[DRAW_BATCH_MAX_VERTS];
    UINT vertexCount = 0;

    int xPos = x;
    int yPos = y;
    const float invW = 1.0f / (float)font->image.width;
    const float invH = 1.0f / (float)font->image.height;

    SaveRenderState();
    mD3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    mD3dDevice->SetTexture(0, font->image.texture);

    const char* p = message;
    while (*p)
    {
        char* cursor = (char*)p;
        uint32_t unicode = ssfn_utf8(&cursor);
        p = cursor;

        std::map<uint32_t, Rect>::const_iterator it = font->charmap.find(unicode);
        if (it == font->charmap.end()) {
            continue;
        }
        
        const Rect& rect = it->second;

        if (vertexCount + 6 > (UINT)DRAW_BATCH_MAX_VERTS)
        {
            mD3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
            vertexCount = 0;
        }

        float uvX = rect.x * invW;
        float uvY = rect.y * invH;
        float uvW = rect.width * invW;
        float uvH = rect.height * invH;
        float u0 = uvX, v0 = uvY, u1 = uvX + uvW, v1 = uvY + uvH;

        float px = (float)xPos - 0.5f;
        float py = (float)yPos - 0.5f;
        float fw = (float)rect.width;
        float fh = (float)rect.height;

        TEXVERTEX* v = &batchBuf[vertexCount];
        v[0].x = px + fw; v[0].y = py;     v[0].z = 0.5f; v[0].rhw = 1.0f; v[0].diffuse = color; v[0].u = u1; v[0].v = v0;
        v[1].x = px + fw; v[1].y = py+fh; v[1].z = 0.5f; v[1].rhw = 1.0f; v[1].diffuse = color; v[1].u = u1; v[1].v = v1;
        v[2].x = px;      v[2].y = py+fh; v[2].z = 0.5f; v[2].rhw = 1.0f; v[2].diffuse = color; v[2].u = u0; v[2].v = v1;
        v[3].x = px + fw; v[3].y = py;     v[3].z = 0.5f; v[3].rhw = 1.0f; v[3].diffuse = color; v[3].u = u1; v[3].v = v0;
        v[4].x = px;      v[4].y = py+fh; v[4].z = 0.5f; v[4].rhw = 1.0f; v[4].diffuse = color; v[4].u = u0; v[4].v = v1;
        v[5].x = px;      v[5].y = py;     v[5].z = 0.5f; v[5].rhw = 1.0f; v[5].diffuse = color; v[5].u = u0; v[5].v = v0;

        vertexCount += 6;
        xPos += rect.width + font->spacing;
    }

    if (vertexCount > 0) {
        mD3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
    }

    RestoreRenderState();
}

void Drawing::DrawFilledRect(uint32_t color, int x, int y, int width, int height)
{
    VERTEX vertices[4];

    float px = (float)x;
    float py = (float)y;
    float fw = (float)width;
    float fh = (float)height;

    vertices[0].x = px;      vertices[0].y = py;      vertices[0].z = 0.5f; vertices[0].rhw = 1.0f; vertices[0].diffuse = color;
    vertices[1].x = px + fw; vertices[1].y = py;      vertices[1].z = 0.5f; vertices[1].rhw = 1.0f; vertices[1].diffuse = color;
    vertices[2].x = px;      vertices[2].y = py + fh; vertices[2].z = 0.5f; vertices[2].rhw = 1.0f; vertices[2].diffuse = color;
    vertices[3].x = px + fw; vertices[3].y = py + fh; vertices[3].z = 0.5f; vertices[3].rhw = 1.0f; vertices[3].diffuse = color;

    SaveRenderState();
    mD3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    mD3dDevice->SetTexture(0, NULL);
    mD3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(VERTEX));
    RestoreRenderState();
}

void Drawing::DrawTexturedRect(D3DTexture* texture, uint32_t diffuse, int x, int y, int width, int height)
{
    TEXVERTEX vertices[4];

    float px = (float)x;
    float py = (float)y;
    float fw = (float)width;
    float fh = (float)height;

    vertices[0].x = px;      vertices[0].y = py;      vertices[0].z = 0.5f; vertices[0].rhw = 1.0f; vertices[0].diffuse = diffuse; vertices[0].u = 0.0f; vertices[0].v = 0.0f;
    vertices[1].x = px + fw; vertices[1].y = py;      vertices[1].z = 0.5f; vertices[1].rhw = 1.0f; vertices[1].diffuse = diffuse; vertices[1].u = 1.0f; vertices[1].v = 0.0f;
    vertices[2].x = px;      vertices[2].y = py + fh; vertices[2].z = 0.5f; vertices[2].rhw = 1.0f; vertices[2].diffuse = diffuse; vertices[2].u = 0.0f; vertices[2].v = 1.0f;
    vertices[3].x = px + fw; vertices[3].y = py + fh; vertices[3].z = 0.5f; vertices[3].rhw = 1.0f; vertices[3].diffuse = diffuse; vertices[3].u = 1.0f; vertices[3].v = 1.0f;

    SaveRenderState();
    mD3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    mD3dDevice->SetTexture(0, texture);
    mD3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    mD3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    mD3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    mD3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(TEXVERTEX));
    RestoreRenderState();
}

void Drawing::DrawNinePatch(D3DTexture* texture, uint32_t diffuse, int x, int y, int width, int height, int cornerWidthPx, int cornerHeightPx, int contentWidthPx, int contentHeightPx)
{
    D3DSURFACE_DESC desc;
    if (FAILED(texture->GetLevelDesc(0, &desc))) return;

    const float surfaceW = (float)desc.Width;
    const float surfaceH = (float)desc.Height;
    const int contentW = contentWidthPx > 0 ? contentWidthPx : (int)surfaceW;
    const int contentH = contentHeightPx > 0 ? contentHeightPx : (int)surfaceH;

    int cw = cornerWidthPx;
    int ch = cornerHeightPx;
    if (cw > contentW / 2) cw = contentW / 2;
    if (ch > contentH / 2) ch = contentH / 2;
    if (cw > width / 2) cw = width / 2;
    if (ch > height / 2) ch = height / 2;

    const float u0 = 0.0f;
    const float u1 = (float)cw / surfaceW;
    const float u2 = (float)(contentW - cw) / surfaceW;
    const float u3 = (float)contentW / surfaceW;
    const float v0 = 0.0f;
    const float v1 = (float)ch / surfaceH;
    const float v2 = (float)(contentH - ch) / surfaceH;
    const float v3 = (float)contentH / surfaceH;

    const float x0 = (float)x;
    const float x1 = (float)(x + cw);
    const float x2 = (float)(x + width - cw);
    const float x3 = (float)(x + width);
    const float y0 = (float)y;
    const float y1 = (float)(y + ch);
    const float y2 = (float)(y + height - ch);
    const float y3 = (float)(y + height);

    TEXVERTEX batchBuf[54];
    int v = 0;

    batchBuf[v].x = x0; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x0; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x0; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v1; v++;

    batchBuf[v].x = x1; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v1; v++;

    batchBuf[v].x = x2; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y0; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v0; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v1; v++;

    batchBuf[v].x = x0; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x0; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x0; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v2; v++;

    batchBuf[v].x = x1; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v2; v++;

    batchBuf[v].x = x2; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y1; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v1; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v2; v++;

    batchBuf[v].x = x0; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x0; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v3; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v3; v++;
    batchBuf[v].x = x0; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u0; batchBuf[v].v = v3; v++;

    batchBuf[v].x = x1; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v3; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v3; v++;
    batchBuf[v].x = x1; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u1; batchBuf[v].v = v3; v++;

    batchBuf[v].x = x2; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v3; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y2; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v2; v++;
    batchBuf[v].x = x3; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u3; batchBuf[v].v = v3; v++;
    batchBuf[v].x = x2; batchBuf[v].y = y3; batchBuf[v].z = 0.5f; batchBuf[v].rhw = 1.0f; batchBuf[v].diffuse = diffuse; batchBuf[v].u = u2; batchBuf[v].v = v3; v++;

    SaveRenderState();
    mD3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    mD3dDevice->SetTexture(0, texture);
    mD3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    mD3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    mD3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    mD3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 18, batchBuf, sizeof(TEXVERTEX));
    RestoreRenderState();
}
