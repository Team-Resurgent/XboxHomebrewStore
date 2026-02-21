#pragma once

#include "Main.h"
#include <map>
#include <string>

class TextureHelper
{
public:
    static bool Init();
    static D3DTexture* LoadFromFile(const std::string filePath);
    static D3DTexture* GetBackground();
    static D3DTexture* GetHeader();
    static D3DTexture* GetFooter();
    static D3DTexture* GetSidebar();
    static D3DTexture* GetCategoryHighlight();
    static D3DTexture* GetCard();
    static D3DTexture* GetCardHighlight();
    static D3DTexture* GetStore();
    static D3DTexture* GetCategoryIcon(const std::string& name);
    static D3DTexture* GetControllerIcon(const std::string& name);
    static D3DTexture* GetScreenshotRef();
    static D3DTexture* GetCoverRef();
private:
    static D3DTexture* CopyTexture(D3DTexture* source);
};
