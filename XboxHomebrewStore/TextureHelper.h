#pragma once

#include "Main.h"

class TextureHelper {
public:
  static bool Init();
  static D3DTexture *LoadFromFile(const std::string filePath);
  static D3DTexture *LoadFromMemory(const uint8_t *data, int32_t size);

  // Load JPEG as DXT1, save .dxt sidecar alongside it, return DXT1 texture.
  // Call on main thread only. On success the .dxt file exists for future loads.
  static D3DTexture *ConvertJpegToDxt(const std::string jpegPath, const std::string dxtPath);

  static D3DTexture *GetBackground();
  static D3DTexture *GetHeader();
  static D3DTexture *GetFooter();
  static D3DTexture *GetSidebar();
  static D3DTexture *GetCategoryHighlight();
  static D3DTexture *GetCard();
  static D3DTexture *GetCardHighlight();
  static D3DTexture *GetNewBadge();
  static D3DTexture *GetUpdateBadge();
  static D3DTexture *GetStore();
  static D3DTexture *GetCategoryIcon(const std::string name);
  static D3DTexture *GetControllerIcon(const std::string name);
  static D3DTexture *GetDriveIcon();
  static D3DTexture *GetFolderIcon();
  static D3DTexture *GetScreenshot();
  static D3DTexture *GetCover();

private:
  static D3DTexture *CopyTexture(D3DTexture *source);
};