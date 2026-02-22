#include "Font.h"
#include "Context.h"
#include "Drawing.h"
#include "Defines.h"
#include "ssfn.h"
#include <map>
#include <string.h>

namespace
{
    ssfn_t      mMainFontContext;
    BitmapFont  mNormalMainFont;
    BitmapFont  mLargeMainFont;

    BitmapFont* GetBitmapFont(FontType font)
    {
        return (font == FONT_NORMAL) ? &mNormalMainFont : &mLargeMainFont;
    }

    float MeasureInternal(BitmapFont* bitmapFont, const std::string& message)
    {
        float width = 0;
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

}

void Font::Init()
{
    if (Drawing::LoadFont("D:\\Media\\Fonts\\Font.sfn", &mMainFontContext))
    {
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 16, 16, 0, 256, &mNormalMainFont);
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 24, 24, 0, 512, &mLargeMainFont);
    }
}

float Font::MeasureText(const FontType font, const std::string& message)
{
    return MeasureInternal(GetBitmapFont(font), message);
}

std::string Font::TruncateText(const FontType font, const std::string& message, float maxWidth)
{
    BitmapFont* bitmapFont = GetBitmapFont(font);

    if (MeasureInternal(bitmapFont, message) <= maxWidth) {
        return message;
    }

    const std::string ellipsis = "...";
    float ellipsisWidth = MeasureInternal(bitmapFont, ellipsis);
    float budget = maxWidth - ellipsisWidth;

    std::string truncated;
    float width = 0;
    const char* p = message.c_str();
    while (*p)
    {
        const char* prev = p;
        char* cursor = (char*)p;
        uint32_t unicode = ssfn_utf8(&cursor);
        p = cursor;

        std::map<uint32_t, Rect>::const_iterator it = bitmapFont->charmap.find(unicode);
        if (it == bitmapFont->charmap.end()) {
            continue;
        }

        int charW = it->second.width + (truncated.empty() ? 0 : bitmapFont->spacing);
        if (width + charW > budget) {
            break;
        }

        truncated.append(prev, p - prev);
        width += charW;
    }

    return truncated + ellipsis;
}

void Font::DrawText(const FontType font, const std::string message, uint32_t color, float x, float y)
{
    Drawing::DrawFont(GetBitmapFont(font), message, color, x, y);
}

void Font::DrawTextScrolling(const FontType font, const std::string& message, uint32_t color, float x, float y, float maxWidth, ScrollState* scrollState)
{
    BitmapFont* bitmapFont = GetBitmapFont(font);

    if (!scrollState->active)
    {
        scrollState->textWidth  = MeasureInternal(bitmapFont, message);
        scrollState->offset     = 0.0f;
        scrollState->lastTick   = GetTickCount();
        scrollState->pauseTimer = SCROLL_PAUSE;
        scrollState->active     = true;
    }

    if (scrollState->textWidth <= maxWidth)
    {
        Drawing::DrawFont(bitmapFont, message, color, x, y);
        return;
    }

    DWORD now = GetTickCount();
    float dt = (float)(now - scrollState->lastTick) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    scrollState->lastTick = now;

    if (scrollState->pauseTimer > 0.0f)
    {
        scrollState->pauseTimer -= dt;
        Drawing::DrawFont(bitmapFont, message, color, x, y);
        return;
    }

    scrollState->offset += SCROLL_SPEED * dt;

    float loopWidth = (float)(scrollState->textWidth + SCROLL_GAP);
    if (scrollState->offset >= loopWidth)
    {
        scrollState->offset    -= loopWidth;
        scrollState->pauseTimer = SCROLL_PAUSE;
    }

    float offsetPx = scrollState->offset;

    Drawing::DrawFont(bitmapFont, message, color, x - offsetPx, y);

    float secondOffset = offsetPx - (scrollState->textWidth + SCROLL_GAP);
    if (secondOffset < maxWidth) {
        Drawing::DrawFont(bitmapFont, message, color, x - secondOffset, y);
    }
}