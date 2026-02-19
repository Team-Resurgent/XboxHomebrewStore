#pragma once

#include "Main.h"
#include <map>
#include <string>

class TextureHelper
{
public:
    static bool Init(D3DDevice* d3dDevice);
    static D3DTexture* LoadFromFile(const std::string filePath);
    static D3DTexture* GetBackground();
    static D3DTexture* GetHeader();
    static D3DTexture* GetFooter();
    static D3DTexture* GetSidebar();
    static D3DTexture* GetCategoryHighlight();
    static D3DTexture* GetStore();
    static D3DTexture* GetCategoryIcon(const std::string& name);
    static D3DTexture* GetControllerIcon(const std::string& name);
    static D3DTexture* GetScreenshot();
    static D3DTexture* GetCover();
    // Shared ref for placeholder (do not Release); use when drawing an item with no icon yet.
    static D3DTexture* GetCoverRef();
private:
    static D3DTexture* CopyTexture(D3DTexture* source);
};
