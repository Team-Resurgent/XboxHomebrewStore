#include "Font.h"
#include "Context.h"
#include "Drawing.h"
#include "ssfn.h"

namespace 
{
    ssfn_t mMainFontContext;
    BitmapFont mNormalMainFont;
    BitmapFont mLargeMainFont;
}

void Font::Init()
{
    if (Drawing::LoadFont("D:\\Media\\Fonts\\Font.sfn", &mMainFontContext))
    {
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 16, 16, 0, 256, &mNormalMainFont);
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 24, 24, 0, 512, &mLargeMainFont);
    }
}

void Font::DrawText(const FontType font, const std::string message, uint32_t color, int x, int y)
{
    Drawing::DrawFont(font == FONT_NORMAL ? &mNormalMainFont : &mLargeMainFont, message, color, x, y);
}