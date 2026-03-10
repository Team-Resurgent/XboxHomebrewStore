#include "TextureHelper.h"
#include "Context.h"
#include "Debug.h"
#include "Defines.h"
#include "String.h"

namespace {
D3DTexture *mBackground = NULL;
D3DTexture *mHeader = NULL;
D3DTexture *mFooter = NULL;
D3DTexture *mSidebar = NULL;
D3DTexture *mStore = NULL;
D3DTexture *mCategoryHighlight = NULL;
D3DTexture *mCard = NULL;
D3DTexture *mCardHighlight = NULL;
D3DTexture *mNewBadge = NULL;
D3DTexture *mUpdateBadge = NULL;
std::map<std::string, D3DTexture *> mCategoryIcons;
std::map<std::string, D3DTexture *> mControllerIcons;
D3DTexture *mDriveIcon = NULL;
D3DTexture *mFolderIcon = NULL;
D3DTexture *mScreenshot = NULL;
D3DTexture *mCover = NULL;
} // namespace

bool TextureHelper::Init() {
  bool result = true;
  mBackground =
      LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Background.jpg"));
  result &= mBackground == NULL;
  mHeader = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Header.png"));
  result &= mHeader == NULL;
  mFooter = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Footer.png"));
  result &= mFooter == NULL;
  mSidebar = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Sidebar.png"));
  result &= mSidebar == NULL;
  mCategoryHighlight =
      LoadFromFile(String::Format("%s%s", MEDIA_PATH, "CategoryHighlight.png"));
  result &= mCategoryHighlight == NULL;
  mCard = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Card.png"));
  result &= mCard == NULL;
  mCardHighlight =
      LoadFromFile(String::Format("%s%s", MEDIA_PATH, "CardHighlight.png"));
  result &= mCardHighlight == NULL;
  mNewBadge = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "NewBadge.png"));
  result &= mNewBadge == NULL;
  mUpdateBadge =
      LoadFromFile(String::Format("%s%s", MEDIA_PATH, "UpdateBadge.png"));
  result &= mUpdateBadge == NULL;
  mStore = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Store.png"));
  result &= mStore == NULL;

  std::string categoriesPath = String::Format("%sCategories\\", MEDIA_PATH);
  std::string pattern = String::Format("%s*.png", categoriesPath.c_str());
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        const char *dot = strrchr(fd.cFileName, '.');
        std::string name =
            dot ? std::string(fd.cFileName, dot - fd.cFileName) : fd.cFileName;
        for (size_t i = 0; i < name.size(); i++) {
          name[i] = (char)tolower((unsigned char)name[i]);
        }
        std::string path =
            String::Format("%s%s", categoriesPath.c_str(), fd.cFileName);
        D3DTexture *tex = LoadFromFile(path);
        if (tex != NULL) {
          mCategoryIcons[name] = tex;
        }
      }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
  }

  std::string controllerPath = String::Format("%sController\\", MEDIA_PATH);
  std::string controllerPattern =
      String::Format("%s*.png", controllerPath.c_str());
  HANDLE hController = FindFirstFileA(controllerPattern.c_str(), &fd);
  if (hController != INVALID_HANDLE_VALUE) {
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        const char *dot = strrchr(fd.cFileName, '.');
        std::string name =
            dot ? std::string(fd.cFileName, dot - fd.cFileName) : fd.cFileName;
        for (size_t i = 0; i < name.size(); i++) {
          name[i] = (char)tolower((unsigned char)name[i]);
        }
        std::string path =
            String::Format("%s%s", controllerPath.c_str(), fd.cFileName);
        D3DTexture *tex = LoadFromFile(path);
        if (tex != NULL) {
          mControllerIcons[name] = tex;
        }
      }
    } while (FindNextFileA(hController, &fd));
    FindClose(hController);
  }

  mDriveIcon = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Drive.png"));
  mFolderIcon = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Folder.png"));

  mScreenshot =
      LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Screenshot.jpg"));
  result &= mScreenshot == NULL;
  mCover = LoadFromFile(String::Format("%s%s", MEDIA_PATH, "Cover.jpg"));
  result &= mCover == NULL;
  return result;
}

// ---------------------------------------------------------------------------
// WriteDxt1DDS -- minimal DDS file writer for a single-mip DXT1 texture.
// 128-byte header (4 magic + 124 DDSURFACEDESC2) followed by raw block data.
// ---------------------------------------------------------------------------
static bool WriteDxt1DDS(const std::string &path, D3DTexture *tex) {
  D3DSURFACE_DESC desc;
  if (FAILED(tex->GetLevelDesc(0, &desc))) {
    return false;
  }

  D3DLOCKED_RECT locked;
  if (FAILED(tex->LockRect(0, &locked, NULL, D3DLOCK_READONLY))) {
    return false;
  }

  // DXT1: 4 bits per pixel, blocks of 4x4 = 8 bytes per block
  UINT bw = (desc.Width  + 3) / 4;
  UINT bh = (desc.Height + 3) / 4;
  UINT dataSize = bw * bh * 8;

  FILE *f = fopen(path.c_str(), "wb");
  if (f == NULL) {
    tex->UnlockRect(0);
    return false;
  }

  // DDS magic 'DDS '
  DWORD magic = 0x20534444;
  fwrite(&magic, 4, 1, f);

  // DDSURFACEDESC2 as 31 DWORDs (124 bytes)
  // hdr[N] index = byte offset within desc / 4
  // DDSURFACEDESC2 layout:
  //   byte  0  hdr[ 0]  dwSize
  //   byte  4  hdr[ 1]  dwFlags
  //   byte  8  hdr[ 2]  dwHeight
  //   byte 12  hdr[ 3]  dwWidth
  //   byte 16  hdr[ 4]  dwPitchOrLinearSize
  //   byte 20  hdr[ 5]  dwDepth
  //   byte 24  hdr[ 6]  dwMipMapCount
  //   byte 28..71       reserved[11]
  //   byte 72  hdr[18]  ddpfPixelFormat.dwSize
  //   byte 76  hdr[19]  ddpfPixelFormat.dwFlags
  //   byte 80  hdr[20]  ddpfPixelFormat.dwFourCC
  //   byte 84..100      ddpfPixelFormat bit masks (0 for fourcc)
  //   byte 104 hdr[26]  ddsCaps.dwCaps1
  DWORD hdr[31];
  memset(hdr, 0, sizeof(hdr));
  hdr[0]  = 124;            // dwSize
  hdr[1]  = 0x00081007;     // CAPS|HEIGHT|WIDTH|PIXELFORMAT|LINEARSIZE
  hdr[2]  = desc.Height;
  hdr[3]  = desc.Width;
  hdr[4]  = dataSize;       // dwPitchOrLinearSize = total compressed bytes
  hdr[6]  = 1;              // dwMipMapCount
  hdr[18] = 32;             // ddpfPixelFormat.dwSize
  hdr[19] = 0x00000004;     // ddpfPixelFormat.dwFlags = DDPF_FOURCC
  hdr[20] = 0x31545844;     // ddpfPixelFormat.dwFourCC = 'DXT1'
  hdr[26] = 0x00001000;     // ddsCaps.dwCaps1 = DDSCAPS_TEXTURE
  fwrite(hdr, sizeof(hdr), 1, f);

  // Raw DXT1 block data
  fwrite(locked.pBits, dataSize, 1, f);

  fclose(f);
  tex->UnlockRect(0);

  Debug::Print("WriteDxt1DDS: wrote %s (%dx%d %d bytes)\n",
      path.c_str(), desc.Width, desc.Height, dataSize);

  // Clear FILE_ATTRIBUTE_ARCHIVE so FileExistsAndAvailable accepts it
  SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);

  return true;
}

D3DTexture *TextureHelper::ConvertJpegToDxt(const std::string jpegPath,
                                             const std::string dxtPath) {
  D3DTexture *tex = NULL;

  // Load JPEG from disk, D3DX compresses to DXT1 during decode
  if (FAILED(D3DXCreateTextureFromFileEx(
          Context::GetD3dDevice(), jpegPath.c_str(),
          D3DX_DEFAULT, D3DX_DEFAULT, 1,
          0, D3DFMT_DXT1, D3DPOOL_DEFAULT,
          D3DX_DEFAULT, D3DX_DEFAULT,
          0, NULL, NULL, &tex))) {
    Debug::Print("ConvertJpegToDxt: D3DX load failed %s\n", jpegPath.c_str());
    return NULL;
  }

  // Save DXT1 data as a .dxt sidecar (standard DDS file, .dxt extension)
  if (WriteDxt1DDS(dxtPath, tex)) {
    Debug::Print("ConvertJpegToDxt: saved %s\n", dxtPath.c_str());
    DeleteFileA(jpegPath.c_str()); // remove .jpg now .dxt is written
  } else {
    Debug::Print("ConvertJpegToDxt: save failed %s\n", dxtPath.c_str());
    // Return texture anyway -- .jpg stays, will re-convert next run
  }

  return tex;
}

D3DTexture *TextureHelper::LoadFromMemory(const uint8_t *data, int32_t size) {
  D3DTexture *tex = NULL;
  if (data == NULL || size <= 0) {
    return NULL;
  }
  if (FAILED(D3DXCreateTextureFromFileInMemoryEx(
          Context::GetD3dDevice(), data, (UINT)size,
          D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
          0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT,
          D3DX_DEFAULT, D3DX_DEFAULT,
          0, NULL, NULL, &tex))) {
    return NULL;
  }
  return tex;
}

D3DTexture *TextureHelper::LoadFromFile(const std::string filePath) {
  D3DTexture *tex = NULL;

  // If the file is a .dxt we know it's a DXT1 DDS -- hint the format
  // so D3DX doesn't have to sniff it and fail on the Xbox
  bool isDxt = filePath.size() >= 4 &&
               filePath.substr(filePath.size() - 4) == ".dxt";
  D3DFORMAT fmt = isDxt ? D3DFMT_DXT1 : D3DFMT_UNKNOWN;

  HRESULT hr = D3DXCreateTextureFromFileEx(
          Context::GetD3dDevice(), filePath.c_str(),
          D3DX_DEFAULT, D3DX_DEFAULT,
          1, 0, fmt, D3DPOOL_DEFAULT,
          D3DX_DEFAULT, D3DX_DEFAULT,
          0, NULL, NULL, &tex);
  if (FAILED(hr)) {
    Debug::Print("LoadFromFile: FAILED %s hr=0x%08X\n",
        filePath.c_str(), (unsigned int)hr);
    return NULL;
  }
  return tex;
}

D3DTexture *TextureHelper::CopyTexture(D3DTexture *source) {
  LPDIRECT3DSURFACE8 pSrcSurf = NULL;
  if (FAILED(source->GetSurfaceLevel(0, &pSrcSurf))) {
    return NULL;
  }
  D3DSURFACE_DESC desc;
  if (FAILED(pSrcSurf->GetDesc(&desc))) {
    pSrcSurf->Release();
    return NULL;
  }
  pSrcSurf->Release();
  pSrcSurf = NULL;

  D3DTexture *pDest = NULL;
  if (FAILED(Context::GetD3dDevice()->CreateTexture(desc.Width, desc.Height, 1,
          0, desc.Format, D3DPOOL_DEFAULT, &pDest))) {
    return NULL;
  }
  LPDIRECT3DSURFACE8 pDstSurf = NULL;
  if (FAILED(pDest->GetSurfaceLevel(0, &pDstSurf))) {
    pDest->Release();
    return NULL;
  }
  if (FAILED(source->GetSurfaceLevel(0, &pSrcSurf))) {
    pDstSurf->Release();
    pDest->Release();
    return NULL;
  }
  RECT rect = {0, 0, (LONG)desc.Width, (LONG)desc.Height};
  POINT point = {0, 0};
  HRESULT hr =
      Context::GetD3dDevice()->CopyRects(pSrcSurf, &rect, 1, pDstSurf, &point);
  pSrcSurf->Release();
  pDstSurf->Release();
  if (FAILED(hr)) {
    pDest->Release();
    return NULL;
  }
  return pDest;
}

D3DTexture *TextureHelper::GetBackground() { return mBackground; }
D3DTexture *TextureHelper::GetHeader() { return mHeader; }
D3DTexture *TextureHelper::GetFooter() { return mFooter; }
D3DTexture *TextureHelper::GetSidebar() { return mSidebar; }
D3DTexture *TextureHelper::GetStore() { return mStore; }
D3DTexture *TextureHelper::GetDriveIcon() { return mDriveIcon; }
D3DTexture *TextureHelper::GetFolderIcon() { return mFolderIcon; }
D3DTexture *TextureHelper::GetCategoryHighlight() { return mCategoryHighlight; }
D3DTexture *TextureHelper::GetCard() { return mCard; }
D3DTexture *TextureHelper::GetCardHighlight() { return mCardHighlight; }
D3DTexture *TextureHelper::GetNewBadge() { return mNewBadge; }
D3DTexture *TextureHelper::GetUpdateBadge() { return mUpdateBadge; }

D3DTexture *TextureHelper::GetCategoryIcon(const std::string name) {
  std::string key = name;
  for (size_t i = 0; i < key.size(); i++) {
    key[i] = (char)tolower((unsigned char)key[i]);
  }
  std::map<std::string, D3DTexture *>::const_iterator it =
      mCategoryIcons.find(key);
  if (it != mCategoryIcons.end()) {
    return it->second;
  }
  it = mCategoryIcons.find("other");
  if (it != mCategoryIcons.end()) {
    return it->second;
  }
  return mCategoryIcons.empty() ? NULL : mCategoryIcons.begin()->second;
}

D3DTexture *TextureHelper::GetControllerIcon(const std::string name) {
  std::string key = name;
  for (size_t i = 0; i < key.size(); i++) {
    key[i] = (char)tolower((unsigned char)key[i]);
  }
  std::map<std::string, D3DTexture *>::const_iterator it =
      mControllerIcons.find(key);
  return (it != mControllerIcons.end()) ? it->second : NULL;
}

D3DTexture *TextureHelper::GetScreenshot() { return mScreenshot; }
D3DTexture *TextureHelper::GetCover() { return mCover; }