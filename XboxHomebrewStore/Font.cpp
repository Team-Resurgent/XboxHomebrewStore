#include "Font.h"
#include "Context.h"
#include "Drawing.h"
#include "Defines.h"
#include "ssfn.h"
namespace
{
    ssfn_t      mMainFontContext;
    BitmapFont  mNormalMainFont;
    BitmapFont  mLargeMainFont;

    BitmapFont* GetBitmapFont(FontType font)
    {
        return (font == FONT_NORMAL) ? &mNormalMainFont : &mLargeMainFont;
    }

    void MeasureInternal(BitmapFont* bitmapFont, const std::string message, float* outWidth)
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
            if (it == bitmapFont->charmap.end()) {
                continue;
            }
            if (!first) {
                width += bitmapFont->spacing;
            }
            width += it->second.width;
            first = false;
        }
        if (outWidth) {
            *outWidth = width;
        }
    }

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

    void MeasureTextWrappedInternal(BitmapFont* font, const std::string message, float maxWidth, float* outWidth, float* outHeight)
    {
        float lineWidth = 0;
        float maxLineWidth = 0;
        float maxHeight = (float)font->line_height;
        const char* p = message.c_str();
        while (*p)
        {
            char* cursor = (char*)p;
            uint32_t unicode = ssfn_utf8(&cursor);
            p = cursor;

            if (unicode == '\n')
            {
                if (lineWidth > maxLineWidth) {
                    maxLineWidth = lineWidth;
                }
                lineWidth = 0;
                maxHeight += (float)font->line_height;
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
                    if (lineWidth > maxLineWidth) {
                        maxLineWidth = lineWidth;
                    }
                    lineWidth = 0;
                    maxHeight += (float)font->line_height;
                    continue;
                }
                lineWidth += (lineWidth > 0 ? (float)font->spacing : 0) + (float)rect.width;
            }
            else
            {
                if (lineWidth > 0 && lineWidth + charWidth > maxWidth)
                {
                    if (lineWidth > maxLineWidth) {
                        maxLineWidth = lineWidth;
                    }
                    lineWidth = 0;
                    maxHeight += (float)font->line_height;
                    charWidth = (float)rect.width;
                }
                lineWidth += charWidth;
            }
        }
        if (lineWidth > maxLineWidth) {
            maxLineWidth = lineWidth;
        }
        if (outWidth) {
            *outWidth = maxLineWidth;
        }
        if (outHeight) {
            *outHeight = maxHeight;
        }
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

void Font::MeasureText(const FontType font, const std::string message, float* outWidth)
{
    MeasureInternal(GetBitmapFont(font), message, outWidth);
}

void Font::MeasureTextWrapped(const FontType font, const std::string message, float maxWidth, float* outWidth, float* outHeight)
{
    MeasureTextWrappedInternal(GetBitmapFont(font), message, maxWidth, outWidth, outHeight);
}

std::string Font::TruncateText(const FontType font, const std::string message, float maxWidth)
{
    BitmapFont* bitmapFont = GetBitmapFont(font);

    float textWidth;
    MeasureInternal(bitmapFont, message, &textWidth);
    if (textWidth <= maxWidth) {
        return message;
    }

    const std::string ellipsis = "...";
    float ellipsisWidth;
    MeasureInternal(bitmapFont, ellipsis, &ellipsisWidth);
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

        int32_t charW = it->second.width + (truncated.empty() ? 0 : bitmapFont->spacing);
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

void Font::DrawTextWrapped(const FontType font, const std::string message, uint32_t color, float x, float y, float maxWidth)
{
    Drawing::DrawFontWrapped(GetBitmapFont(font), message, color, x, y, maxWidth);
}

void Font::DrawTextScrolling(const FontType font, const std::string message, uint32_t color, float x, float y, float maxWidth, ScrollState& scrollState)
{
    BitmapFont* bitmapFont = GetBitmapFont(font);

    if (!scrollState.active)
    {
        MeasureInternal(bitmapFont, message, &scrollState.textWidth);
        scrollState.offset     = 0.0f;
        scrollState.lastTick   = GetTickCount();
        scrollState.pauseTimer = SCROLL_PAUSE;
        scrollState.active     = true;
    }

    if (scrollState.textWidth <= maxWidth)
    {
        Drawing::DrawFont(bitmapFont, message, color, x, y);
        return;
    }

    DWORD now = GetTickCount();
    float dt = (float)(now - scrollState.lastTick) / 1000.0f;
    if (dt > 0.1f) {
        dt = 0.1f;
    }
    scrollState.lastTick = now;

    if (scrollState.pauseTimer > 0.0f)
    {
        scrollState.pauseTimer -= dt;
        Drawing::DrawFont(bitmapFont, message, color, x, y);
        return;
    }

    scrollState.offset += SCROLL_SPEED * dt;

    float loopWidth = (float)(scrollState.textWidth + SCROLL_GAP);
    if (scrollState.offset >= loopWidth)
    {
        scrollState.offset    -= loopWidth;
        scrollState.pauseTimer = SCROLL_PAUSE;
    }

    float offsetPx = scrollState.offset;

    Drawing::DrawFont(bitmapFont, message, color, x - offsetPx, y);

    float secondOffset = offsetPx - (scrollState.textWidth + SCROLL_GAP);
    if (secondOffset < maxWidth) {
        Drawing::DrawFont(bitmapFont, message, color, x - secondOffset, y);
    }
}