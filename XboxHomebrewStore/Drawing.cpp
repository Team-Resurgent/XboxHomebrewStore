#include "Drawing.h"
#include "Context.h"

#define SSFN_IMPLEMENTATION
#define SFFN_MAXLINES 8192
#define SSFN_memcmp memcmp
#define SSFN_memset memset
#define SSFN_realloc realloc
#define SSFN_free free
#include "ssfn.h"

#define DRAW_BATCH_MAX_VERTS 16380
#define SAVE_STATE_COUNT 21

namespace 
{
    uint32_t mSavedStateIndex;
    uint32_t mSavedState[4 * SAVE_STATE_COUNT];

    float MeasureWordWidth(BitmapFont* font, const char* p, const char** outEnd)
    {
        float width = 0;
        bool first = true;
        while (*p)
        {
            char* cursor = (char*)p;
            uint32_t unicode = ssfn_utf8(&cursor);
            if (unicode == ' ' || unicode == '\n' || unicode == '\t') {
                break;
            }
            std::map<uint32_t, Rect>::const_iterator it = font->charmap.find(unicode);
            if (it != font->charmap.end())
            {
                if (!first) {
                    width += (float)font->spacing;
                }
                width += (float)it->second.width;
                first = false;
            }
            p = cursor;
        }
        *outEnd = p;
        return width;
    }
}

void Drawing::Init()
{
    mSavedStateIndex = 0;
}

void Drawing::SaveRenderState()
{
    uint32_t* savedStatePage = &mSavedState[mSavedStateIndex];
    mSavedStateIndex += SAVE_STATE_COUNT;

    Context::GetD3dDevice()->GetRenderState(D3DRS_ALPHABLENDENABLE, &savedStatePage[0]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_SRCBLEND, &savedStatePage[1]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_DESTBLEND, &savedStatePage[2]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_ALPHATESTENABLE, &savedStatePage[3]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_ALPHAREF, &savedStatePage[4]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_ALPHAFUNC, &savedStatePage[5]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_FILLMODE, &savedStatePage[6]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_CULLMODE, &savedStatePage[7]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_ZENABLE, &savedStatePage[8]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_STENCILENABLE, &savedStatePage[9]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_STENCILFUNC, &savedStatePage[10]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_STENCILREF, &savedStatePage[11]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_STENCILMASK, &savedStatePage[12]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_STENCILWRITEMASK, &savedStatePage[13]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_STENCILPASS, &savedStatePage[14]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_COLORWRITEENABLE, &savedStatePage[15]);
    Context::GetD3dDevice()->GetRenderState(D3DRS_EDGEANTIALIAS, &savedStatePage[16]);
    Context::GetD3dDevice()->GetTextureStageState(0, D3DTSS_MINFILTER, &savedStatePage[17]);
    Context::GetD3dDevice()->GetTextureStageState(0, D3DTSS_MAGFILTER, &savedStatePage[18]);
    Context::GetD3dDevice()->GetTextureStageState(0, D3DTSS_COLOROP, &savedStatePage[19]);
    Context::GetD3dDevice()->GetTextureStageState(0, D3DTSS_COLORARG1, &savedStatePage[20]);
    Context::GetD3dDevice()->GetTextureStageState(0, D3DTSS_COLORARG2, &savedStatePage[21]);
}

void Drawing::RestoreRenderState()
{
    mSavedStateIndex -= SAVE_STATE_COUNT;
    uint32_t* savedStatePage = &mSavedState[mSavedStateIndex];

    Context::GetD3dDevice()->SetTexture(0, nullptr);

    Context::GetD3dDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, savedStatePage[0]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_SRCBLEND, savedStatePage[1]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_DESTBLEND, savedStatePage[2]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, savedStatePage[3]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_ALPHAREF, savedStatePage[4]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_ALPHAFUNC, savedStatePage[5]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_FILLMODE, savedStatePage[6]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_CULLMODE, savedStatePage[7]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_ZENABLE, savedStatePage[8]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILENABLE, savedStatePage[9]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILFUNC, savedStatePage[10]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILREF, savedStatePage[11]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILMASK, savedStatePage[12]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILWRITEMASK, savedStatePage[13]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILPASS, savedStatePage[14]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_COLORWRITEENABLE, savedStatePage[15]);
    Context::GetD3dDevice()->SetRenderState(D3DRS_EDGEANTIALIAS, savedStatePage[16]);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_MINFILTER, savedStatePage[17]);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_MAGFILTER, savedStatePage[18]);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLOROP, savedStatePage[19]);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, savedStatePage[20]);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, savedStatePage[21]);
}

void Drawing::Swizzle(const void* src, const uint32_t& depth, const uint32_t& width, const uint32_t& height, void* dest) 
{
    for (UINT y = 0; y < height; y++) {
        UINT sy = 0;
        if (y < width) {
            for (int32_t bit = 0; bit < 16; bit++)
                sy |= ((y >> bit) & 1) << (2 * bit);
            sy <<= 1; // y counts twice
        } else {
            UINT y_mask = y % width;
            for (int32_t bit = 0; bit < 16; bit++)
                sy |= ((y_mask >> bit) & 1) << (2 * bit);
            sy <<= 1; // y counts twice
            sy += (y / width) * width * width;
        }
        BYTE* s = (BYTE*)src + y * width * depth;
        for (UINT x = 0; x < width; x++) {
            UINT sx = 0;
            if (x < height * 2) {
                for (int32_t bit = 0; bit < 16; bit++)
                    sx |= ((x >> bit) & 1) << (2 * bit);
            } else {
                int32_t x_mask = x % (2 * height);
                for (int32_t bit = 0; bit < 16; bit++)
                    sx |= ((x_mask >> bit) & 1) << (2 * bit);
                sx += (x / (2 * height)) * 2 * height * height;
            }
            BYTE* d = (BYTE*)dest + (sx + sy) * depth;
            for (unsigned int i = 0; i < depth; ++i)
                *d++ = *s++;
        }
    }
}

bool Drawing::TryCreateImage(uint8_t* imageData, D3DFORMAT format, int32_t width, int32_t height, Image* image) 
{
    image->width = width;
    image->height = height;

    if (FAILED(D3DXCreateTexture(Context::GetD3dDevice(), image->width, image->height, 1, 0, format, D3DPOOL_DEFAULT, &image->texture))) {
        return false;
    }

    D3DSURFACE_DESC surfaceDesc;
    image->texture->GetLevelDesc(0, &surfaceDesc);
    image->uv_width = image->width / (float)surfaceDesc.Width;
    image->uv_height = image->height / (float)surfaceDesc.Height;
    
    D3DLOCKED_RECT lockedRect;
    if (SUCCEEDED(image->texture->LockRect(0, &lockedRect, nullptr, 0))) {
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


bool Drawing::TryGenerateBitmapFont(void* context, const std::string fontName, int32_t fontStyle, int32_t fontSize, int32_t lineHeight, int32_t spacing, int32_t textureDimension, BitmapFont* bitmapFont) 
{
    ssfn_select((ssfn_t*)context, SSFN_FAMILY_ANY, fontName.c_str(), (int)fontStyle, (int)fontSize);

    int32_t textureWidth = textureDimension;
    int32_t textureHeight = textureDimension;

    uint32_t* imageData = (uint32_t*)malloc(textureWidth * textureHeight * 4);
    memset(imageData, 0, textureWidth * textureHeight * 4);

    int32_t x = 2;
    int32_t y = 2;

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
        buffer.p = (uint16_t)(textureWidth * 4);
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

void Drawing::DrawFont(BitmapFont* font, const std::string& message, uint32_t color, float x, float y)
{
    static TEXVERTEX batchBuf[DRAW_BATCH_MAX_VERTS];
    UINT vertexCount = 0;

    float xPos = x;
    float yPos = y;
    const float invW = 1.0f / (float)font->image.width;
    const float invH = 1.0f / (float)font->image.height;

    SaveRenderState();
    Context::GetD3dDevice()->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    Context::GetD3dDevice()->SetTexture(0, font->image.texture);

    const char* p = message.c_str();
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
            Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
            vertexCount = 0;
        }

        float uvX = rect.x * invW;
        float uvY = rect.y * invH;
        float uvW = rect.width * invW;
        float uvH = rect.height * invH;
        float u0 = uvX, v0 = uvY, u1 = uvX + uvW, v1 = uvY + uvH;

        float px = xPos - 0.5f;
        float py = yPos - 0.5f;
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
        Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
    }

    RestoreRenderState();
}

void Drawing::DrawFontWrapped(BitmapFont* font, const std::string& message, uint32_t color, float x, float y, float maxWidth)
{
    static TEXVERTEX batchBuf[DRAW_BATCH_MAX_VERTS];
    UINT vertexCount = 0;

    float xPos = x;
    float yPos = y;
    float lineWidth = 0;
    const float invW = 1.0f / (float)font->image.width;
    const float invH = 1.0f / (float)font->image.height;

    SaveRenderState();
    Context::GetD3dDevice()->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    Context::GetD3dDevice()->SetTexture(0, font->image.texture);

    const char* p = message.c_str();
    while (*p)
    {
        char* cursor = (char*)p;
        uint32_t unicode = ssfn_utf8(&cursor);
        p = cursor;

        if (unicode == '\n')
        {
            xPos = x;
            yPos += (float)font->line_height;
            lineWidth = 0;
            continue;
        }

        std::map<uint32_t, Rect>::const_iterator it = font->charmap.find(unicode);
        if (it == font->charmap.end()) {
            continue;
        }

        const Rect& rect = it->second;
        float charWidth = (float)rect.width + (lineWidth > 0 ? (float)font->spacing : 0);

        if (unicode == ' ')
        {
            const char* nextWordStart = p;
            float nextWordW = MeasureWordWidth(font, nextWordStart, &nextWordStart);
            if (lineWidth > 0 && lineWidth + (float)font->spacing + (float)rect.width + nextWordW > maxWidth)
            {
                xPos = x;
                yPos += (float)font->line_height;
                lineWidth = 0;
                continue;
            }
        }
        else if (lineWidth > 0 && lineWidth + charWidth > maxWidth)
        {
            xPos = x;
            yPos += (float)font->line_height;
            lineWidth = 0;
            charWidth = (float)rect.width;
        }

        if (vertexCount + 6 > (UINT)DRAW_BATCH_MAX_VERTS)
        {
            Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
            vertexCount = 0;
        }

        float uvX = rect.x * invW;
        float uvY = rect.y * invH;
        float uvW = rect.width * invW;
        float uvH = rect.height * invH;
        float u0 = uvX, v0 = uvY, u1 = uvX + uvW, v1 = uvY + uvH;

        float px = xPos - 0.5f;
        float py = yPos - 0.5f;
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
        xPos += (float)rect.width + (lineWidth > 0 ? (float)font->spacing : 0);
        lineWidth += charWidth;
    }

    if (vertexCount > 0) {
        Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
    }

    RestoreRenderState();
}

void Drawing::DrawFilledRect(uint32_t color, float x, float y, float width, float height)
{
    VERTEX vertices[4];

    float px = x;
    float py = y;
    float fw = width;
    float fh = height;

    vertices[0].x = px;      vertices[0].y = py;      vertices[0].z = 0.5f; vertices[0].rhw = 1.0f; vertices[0].diffuse = color;
    vertices[1].x = px + fw; vertices[1].y = py;      vertices[1].z = 0.5f; vertices[1].rhw = 1.0f; vertices[1].diffuse = color;
    vertices[2].x = px;      vertices[2].y = py + fh; vertices[2].z = 0.5f; vertices[2].rhw = 1.0f; vertices[2].diffuse = color;
    vertices[3].x = px + fw; vertices[3].y = py + fh; vertices[3].z = 0.5f; vertices[3].rhw = 1.0f; vertices[3].diffuse = color;

    SaveRenderState();
    Context::GetD3dDevice()->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    Context::GetD3dDevice()->SetTexture(0, nullptr);
    Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(VERTEX));
    RestoreRenderState();
}

void Drawing::DrawTexturedRect(D3DTexture* texture, uint32_t diffuse, float x, float y, float width, float height)
{
    TEXVERTEX vertices[4];

    float px = x;
    float py = y;
    float fw = width;
    float fh = height;

    vertices[0].x = px;      vertices[0].y = py;      vertices[0].z = 0.5f; vertices[0].rhw = 1.0f; vertices[0].diffuse = diffuse; vertices[0].u = 0.0f; vertices[0].v = 0.0f;
    vertices[1].x = px + fw; vertices[1].y = py;      vertices[1].z = 0.5f; vertices[1].rhw = 1.0f; vertices[1].diffuse = diffuse; vertices[1].u = 1.0f; vertices[1].v = 0.0f;
    vertices[2].x = px;      vertices[2].y = py + fh; vertices[2].z = 0.5f; vertices[2].rhw = 1.0f; vertices[2].diffuse = diffuse; vertices[2].u = 0.0f; vertices[2].v = 1.0f;
    vertices[3].x = px + fw; vertices[3].y = py + fh; vertices[3].z = 0.5f; vertices[3].rhw = 1.0f; vertices[3].diffuse = diffuse; vertices[3].u = 1.0f; vertices[3].v = 1.0f;

    SaveRenderState();
    Context::GetD3dDevice()->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    Context::GetD3dDevice()->SetTexture(0, texture);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(TEXVERTEX));
    RestoreRenderState();
}

void Drawing::DrawNinePatch(D3DTexture* texture, uint32_t diffuse, float x, float y, float width, float height, float cornerWidthPx, float cornerHeightPx, float contentWidthPx, float contentHeightPx)
{
    D3DSURFACE_DESC desc;
    if (FAILED(texture->GetLevelDesc(0, &desc))) {
        return;
    }

    const float surfaceW = (float)desc.Width;
    const float surfaceH = (float)desc.Height;
    const float contentW = contentWidthPx > 0 ? contentWidthPx : (float)(int32_t)surfaceW;
    const float contentH = contentHeightPx > 0 ? contentHeightPx : (float)(int32_t)surfaceH;

    float cw = cornerWidthPx;
    float ch = cornerHeightPx;
    if (cw > contentW / 2) {
        cw = contentW / 2;
    }
    if (ch > contentH / 2) {
        ch = contentH / 2;
    }
    if (cw > width / 2) {
        cw = width / 2;
    }
    if (ch > height / 2) {
        ch = height / 2;
    }

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
    int32_t v = 0;

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
    Context::GetD3dDevice()->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    Context::GetD3dDevice()->SetTexture(0, texture);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    Context::GetD3dDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 18, batchBuf, sizeof(TEXVERTEX));
    RestoreRenderState();
}

void Drawing::BeginStencil(float x, float y, float w, float h)
{
    SaveRenderState();

    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILREF, 1);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    Context::GetD3dDevice()->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
    Context::GetD3dDevice()->SetRenderState(D3DRS_ZENABLE, FALSE);

    struct stencil_vertex { float x,y,z,rhw; };
    stencil_vertex quad[4] = {
        { x, y, 0.0f, 1.0f },
        { x + w, y, 0.0f, 1.0f },
        { x, y + h, 0.0f, 1.0f },
        { x + w, y + h, 0.0f, 1.0f },
    };

    Context::GetD3dDevice()->Clear(0L, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0L);
    Context::GetD3dDevice()->SetVertexShader(D3DFVF_XYZRHW);
    Context::GetD3dDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(stencil_vertex));

    Context::GetD3dDevice()->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILREF, 1);
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
}

void Drawing::EndStencil()
{
    Context::GetD3dDevice()->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    RestoreRenderState();
}
