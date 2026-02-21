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
    static void Init();

    // Draw text normally
    static void DrawText(const FontType font, const std::string message, uint32_t color, int x, int y);

    // Returns the pixel width of a string
    static int MeasureText(const FontType font, const std::string& message);

    // Returns a truncated copy of message that fits within maxWidth, with "..." appended if cut.
    // Call this once at data-load time and cache the result rather than calling per-frame.
    static std::string TruncateText(const FontType font, const std::string& message, int maxWidth);

    // Draw text clipped to maxWidth pixels. Adds "..." if truncated.
    //static void DrawTextClipped(const FontType font, const std::string& message, uint32_t color, int x, int y, int maxWidth);

    // Draw scrolling text that fits within maxWidth.
    // When the text is short enough to fit it draws normally.
    // When it is too long it scrolls left continuously.
    // scrollKey is a unique integer per tile so each tile has its own scroll position.
    static void DrawTextScrolling(const FontType font, const std::string& message, uint32_t color, int x, int y, int maxWidth, int scrollKey);

    // Call this when an item is deselected to reset its scroll position
    static void ResetScroll(int scrollKey);
};