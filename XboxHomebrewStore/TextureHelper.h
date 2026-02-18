#pragma once

#include "Main.h"

class TextureHelper
{
public:
    static bool Init(D3DDevice* d3dDevice);
    static D3DTexture* LoadFromFile(const std::string filePath);
    static D3DTexture* GetBackground();
    static D3DTexture* GetScreenshot();
    static D3DTexture* GetCover();
private:
    static D3DTexture* CopyTexture(D3DTexture* source);
};
