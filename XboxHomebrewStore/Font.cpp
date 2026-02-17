#include "Font.h"
#include "Drawing.h"
#include "ssfn.h"

namespace 
{
    D3DDevice* mD3dDevice;
    ssfn_t mMainFontContext;
    BitmapFont mMainFont;
}

void Font::Init(D3DDevice* d3dDevice)
{
    mD3dDevice = d3dDevice;

    if (Drawing::LoadFont("D:\\Media\\Fonts\\Font.sfn", &mMainFontContext))
    {
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 16, 16, 0, 256, &mMainFont);
    }
}

void Font::DrawText(const char* message, uint32_t color, int x, int y)
{
    Drawing::DrawFont(&mMainFont, message, color, x, y);
}