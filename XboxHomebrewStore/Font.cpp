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
        Drawing::TryGenerateBitmapFont(&mMainFontContext, "FreeSans", SSFN_STYLE_REGULAR, 32, 32, 0, 1024, &mMainFont);
    }

    while(true)
    {
        Drawing::DrawFont(&mMainFont, "Hello World", 0xffffffff, 50, 50);
    }
}