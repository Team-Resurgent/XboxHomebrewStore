#include "Font.h"
#include "Context.h"
#include "Drawing.h"
#include "ssfn.h"
#include <map>
#include <string.h>

typedef struct
{
    float   offset;
    DWORD   lastTick;
    float   pauseTimer;
    int     textWidth;
    bool    active;
} ScrollState;

#define MAX_SCROLL_SLOTS 32
#define SCROLL_SPEED 40.0f
#define SCROLL_PAUSE 1.5f
#define SCROLL_GAP 48

namespace
{
    ssfn_t      mMainFontContext;
    BitmapFont  mNormalMainFont;
    BitmapFont  mLargeMainFont;

    ScrollState mScrollStates[MAX_SCROLL_SLOTS];

    BitmapFont* GetBitmapFont(FontType font)
    {
        return (font == FONT_NORMAL) ? &mNormalMainFont : &mLargeMainFont;
    }

    int MeasureInternal(BitmapFont* bitmapFont, const std::string& message)
    {
        int width = 0;
        const char* p = message.c_str();
        bool first = true;
        while (*p)
        {
            char* cursor = (char*)p;
            uint32_t unicode = ssfn_utf8(&cursor);
            p = cursor;
            std::map<uint32_t, Rect>::const_iterator it = bitmapFont->charmap.find(unicode);
            if (it == bitmapFont->charmap.end())
                continue;
            if (!first)
                width += bitmapFont->spacing;
            width += it->second.width;
            first = false;
        }
        return width;
    }

    /*void DrawFontClipped(BitmapFont* font, const std::string& message, uint32_t color, int x, int y, int clipWidth, int offsetX)
    {
        int clipLeft  = x;
        int clipRight = x + clipWidth;
        int xPos      = x - offsetX;

        const float invW = 1.0f / (float)font->image.width;
        const float invH = 1.0f / (float)font->image.height;

        static TEXVERTEX batchBuf[2048];
        UINT vertexCount = 0;

        LPDIRECT3DDEVICE8 dev = Context::GetD3dDevice();
        dev->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        dev->SetTexture(0, font->image.texture);

        const char* p = message.c_str();
        while (*p)
        {
            char* cursor = (char*)p;
            uint32_t unicode = ssfn_utf8(&cursor);
            p = cursor;

            std::map<uint32_t, Rect>::const_iterator it = font->charmap.find(unicode);
            if (it == font->charmap.end())
                continue;

            const Rect& rect = it->second;
            int charLeft  = xPos;
            int charRight = xPos + rect.width;

            if (charRight <= clipLeft || charLeft >= clipRight)
            {
                xPos += rect.width + font->spacing;
                continue;
            }

            float uvX = rect.x * invW;
            float uvY = rect.y * invH;
            float uvW = rect.width  * invW;
            float uvH = rect.height * invH;

            float drawLeft  = (float)charLeft;
            float drawRight = (float)charRight;
            float u0 = uvX;
            float u1 = uvX + uvW;

            if (drawLeft < (float)clipLeft)
            {
                float excess = (float)clipLeft - drawLeft;
                float frac   = excess / (float)rect.width;
                u0      += frac * uvW;
                drawLeft = (float)clipLeft;
            }
            if (drawRight > (float)clipRight)
            {
                float excess = drawRight - (float)clipRight;
                float frac   = excess / (float)rect.width;
                u1       -= frac * uvW;
                drawRight = (float)clipRight;
            }

            float px = drawLeft  - 0.5f;
            float qx = drawRight - 0.5f;
            float py = (float)y  - 0.5f;
            float fh = (float)rect.height;
            float v0 = uvY;
            float v1 = uvY + uvH;

            if (vertexCount + 6 > 2048)
            {
                dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
                vertexCount = 0;
            }

            TEXVERTEX* v = &batchBuf[vertexCount];
            v[0].x = qx; v[0].y = py;    v[0].z = 0.5f; v[0].rhw = 1.0f; v[0].diffuse = color; v[0].u = u1; v[0].v = v0;
            v[1].x = qx; v[1].y = py+fh; v[1].z = 0.5f; v[1].rhw = 1.0f; v[1].diffuse = color; v[1].u = u1; v[1].v = v1;
            v[2].x = px; v[2].y = py+fh; v[2].z = 0.5f; v[2].rhw = 1.0f; v[2].diffuse = color; v[2].u = u0; v[2].v = v1;
            v[3].x = qx; v[3].y = py;    v[3].z = 0.5f; v[3].rhw = 1.0f; v[3].diffuse = color; v[3].u = u1; v[3].v = v0;
            v[4].x = px; v[4].y = py+fh; v[4].z = 0.5f; v[4].rhw = 1.0f; v[4].diffuse = color; v[4].u = u0; v[4].v = v1;
            v[5].x = px; v[5].y = py;    v[5].z = 0.5f; v[5].rhw = 1.0f; v[5].diffuse = color; v[5].u = u0; v[5].v = v0;

            vertexCount += 6;
            xPos += rect.width + font->spacing;
        }

        if (vertexCount > 0) {
            dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, batchBuf, sizeof(TEXVERTEX));
        }
    }*/
}

//-----------------------------------------------------------------------------

void Font::Init()
{
    memset(mScrollStates, 0, sizeof(mScrollStates));

    if (Drawing::LoadFont("D:\\Media\\Fonts\\Font.sfn", &mMainFontContext))
    {
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 16, 16, 0, 256, &mNormalMainFont);
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 24, 24, 0, 512, &mLargeMainFont);
    }
}

void Font::DrawText(const FontType font, const std::string message, uint32_t color, int x, int y)
{
    Drawing::DrawFont(GetBitmapFont(font), message, color, x, y);
}

int Font::MeasureText(const FontType font, const std::string& message)
{
    return MeasureInternal(GetBitmapFont(font), message);
}

std::string Font::TruncateText(const FontType font, const std::string& message, int maxWidth)
{
    BitmapFont* bitmapFont = GetBitmapFont(font);

    if (MeasureInternal(bitmapFont, message) <= maxWidth)
        return message;

    const std::string ellipsis = "...";
    int ellipsisWidth = MeasureInternal(bitmapFont, ellipsis);
    int budget = maxWidth - ellipsisWidth;

    std::string truncated;
    int width = 0;
    const char* p = message.c_str();
    while (*p)
    {
        const char* prev = p;
        char* cursor = (char*)p;
        uint32_t unicode = ssfn_utf8(&cursor);
        p = cursor;

        std::map<uint32_t, Rect>::const_iterator it = bitmapFont->charmap.find(unicode);
        if (it == bitmapFont->charmap.end())
            continue;

        int charW = it->second.width + (truncated.empty() ? 0 : bitmapFont->spacing);
        if (width + charW > budget)
            break;

        truncated.append(prev, p - prev);
        width += charW;
    }

    return truncated + ellipsis;
}

//void Font::DrawTextClipped(const FontType font, const std::string& message, uint32_t color, int x, int y, int maxWidth)
//{
//    BitmapFont* bitmapFont = GetBitmapFont(font);
//    if (MeasureInternal(bitmapFont, message) <= maxWidth)
//    {
//        Drawing::DrawFont(bitmapFont, message, color, x, y);
//        return;
//    }
//
//    const std::string ellipsis = "...";
//    int ellipsisWidth = MeasureInternal(bitmapFont, ellipsis);
//    int budget = maxWidth - ellipsisWidth;
//
//    std::string truncated;
//    int width = 0;
//    const char* p = message.c_str();
//    while (*p)
//    {
//        const char* prev = p;
//        char* cursor = (char*)p;
//        uint32_t unicode = ssfn_utf8(&cursor);
//        p = cursor;
//
//        std::map<uint32_t, Rect>::const_iterator it = bitmapFont->charmap.find(unicode);
//        if (it == bitmapFont->charmap.end())
//            continue;
//
//        int charW = it->second.width + (truncated.empty() ? 0 : bitmapFont->spacing);
//        if (width + charW > budget)
//            break;
//
//        truncated.append(prev, p - prev);
//        width += charW;
//    }
//
//    truncated += ellipsis;
//    Drawing::DrawFont(bitmapFont, truncated, color, x, y);
//}
//
void Font::DrawTextScrolling(const FontType font, const std::string& message, uint32_t color, int x, int y, int maxWidth, int scrollKey)
{
    // Guard against out-of-range keys
    if (scrollKey < 0 || scrollKey >= MAX_SCROLL_SLOTS)
    {
        Drawing::DrawFont(GetBitmapFont(font), message, color, x, y);
        return;
    }

    BitmapFont* bitmapFont = GetBitmapFont(font);
    ScrollState* state = &mScrollStates[scrollKey];

    // Measure once when first activated, then cache it
    if (!state->active)
    {
        state->textWidth  = MeasureInternal(bitmapFont, message);
        state->offset     = 0.0f;
        state->lastTick   = GetTickCount();
        state->pauseTimer = SCROLL_PAUSE;
        state->active     = true;
    }

    // If it fits, just draw normally - no scrolling needed
    if (state->textWidth <= maxWidth)
    {
        Drawing::DrawFont(bitmapFont, message, color, x, y);
        return;
    }

    DWORD now = GetTickCount();
    float dt = (float)(now - state->lastTick) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    state->lastTick = now;

    if (state->pauseTimer > 0.0f)
    {
        state->pauseTimer -= dt;
        // During pause draw the truncated display string to avoid overflow
        Drawing::DrawFont(bitmapFont, message, color, x, y);
        return;
    }

    state->offset += SCROLL_SPEED * dt;

    float loopWidth = (float)(state->textWidth + SCROLL_GAP);
    if (state->offset >= loopWidth)
    {
        state->offset    -= loopWidth;
        state->pauseTimer = SCROLL_PAUSE;
    }

    int offsetPx = (int)state->offset;

    Drawing::DrawFont(bitmapFont, message, color, x - offsetPx, y);

    // Draw second copy entering from right for seamless loop
    int secondOffset = offsetPx - (state->textWidth + SCROLL_GAP);
    if (secondOffset < maxWidth) {
        Drawing::DrawFont(bitmapFont, message, color, x - secondOffset, y);
    }
}

void Font::ResetScroll(int scrollKey)
{
    if (scrollKey >= 0 && scrollKey < MAX_SCROLL_SLOTS) {
        mScrollStates[scrollKey].active = false;
    }
}