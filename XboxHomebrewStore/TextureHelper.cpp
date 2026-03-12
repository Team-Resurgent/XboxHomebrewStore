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
// WriteDxt1DDS -- writes a DXT1 texture to a DDS file. Main thread only.
// ---------------------------------------------------------------------------
static bool WriteDxt1DDS(const std::string &path, D3DTexture *tex) {
  D3DSURFACE_DESC desc;
  if (FAILED(tex->GetLevelDesc(0, &desc))) return false;

  D3DLOCKED_RECT locked;
  if (FAILED(tex->LockRect(0, &locked, NULL, D3DLOCK_READONLY))) return false;

  UINT bw = (desc.Width  + 3) / 4;
  UINT bh = (desc.Height + 3) / 4;
  UINT dataSize = bw * bh * 8;

  FILE *f = fopen(path.c_str(), "wb");
  if (f == NULL) { tex->UnlockRect(0); return false; }

  DWORD magic = 0x20534444;
  fwrite(&magic, 4, 1, f);

  DWORD hdr[31];
  memset(hdr, 0, sizeof(hdr));
  hdr[0]  = 124;
  hdr[1]  = 0x00081007;
  hdr[2]  = desc.Height;
  hdr[3]  = desc.Width;
  hdr[4]  = dataSize;
  hdr[6]  = 1;
  hdr[18] = 32;
  hdr[19] = 0x00000004;
  hdr[20] = 0x31545844;  // 'DXT1'
  hdr[26] = 0x00001000;
  fwrite(hdr, sizeof(hdr), 1, f);
  fwrite(locked.pBits, dataSize, 1, f);
  fclose(f);
  tex->UnlockRect(0);

  SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
  Debug::Print("WriteDxt1DDS: wrote %s (%dx%d %d bytes)\n",
      path.c_str(), desc.Width, desc.Height, dataSize);
  return true;
}


// ---------------------------------------------------------------------------
// LoadFromFile -- for .dxt files uses CreateTexture+LockRect+memcpy+UnlockRect
// (pure GPU upload, no D3DX overhead). Other formats fall back to D3DXCreateTextureFromFileEx.
// ---------------------------------------------------------------------------
D3DTexture *TextureHelper::LoadFromFile(const std::string filePath) {
  bool isDxt = filePath.size() >= 4 &&
               filePath.substr(filePath.size() - 4) == ".dxt";

  if (isDxt) {
    // Read the raw DDS file ourselves and upload via LockRect -- fastest possible path
    FILE *f = fopen(filePath.c_str(), "rb");
    if (f == NULL) {
      Debug::Print("LoadFromFile: fopen failed %s\n", filePath.c_str());
      return NULL;
    }

    // DDS layout: 4 magic + 124 header = 128 bytes, then raw DXT1 blocks
    DWORD magic = 0;
    fread(&magic, 4, 1, f);
    if (magic != 0x20534444) { // 'DDS '
      Debug::Print("LoadFromFile: bad DDS magic %s\n", filePath.c_str());
      fclose(f);
      DeleteFileA(filePath.c_str()); // corrupt file -- delete so it re-downloads
      return NULL;
    }

    DWORD hdr[31];
    fread(hdr, sizeof(hdr), 1, f);

    UINT width    = hdr[3];
    UINT height   = hdr[2];
    UINT dataSize = hdr[4];

    if (width == 0 || height == 0 || dataSize == 0) {
      Debug::Print("LoadFromFile: bad DDS dims %s\n", filePath.c_str());
      fclose(f);
      DeleteFileA(filePath.c_str()); // corrupt file -- delete so it re-downloads
      return NULL;
    }

    void *blocks = malloc(dataSize);
    if (blocks == NULL) {
      Debug::Print("LoadFromFile: malloc failed %s\n", filePath.c_str());
      fclose(f);
      return NULL;
    }

    fread(blocks, dataSize, 1, f);
    fclose(f);

    D3DTexture *tex = NULL;
    HRESULT hr = Context::GetD3dDevice()->CreateTexture(
        width, height, 1, 0, D3DFMT_DXT1, D3DPOOL_DEFAULT, &tex);
    if (FAILED(hr) || tex == NULL) {
      Debug::Print("LoadFromFile: CreateTexture failed %s hr=0x%08X\n",
          filePath.c_str(), (unsigned int)hr);
      free(blocks);
      return NULL;
    }

    D3DLOCKED_RECT locked;
    hr = tex->LockRect(0, &locked, NULL, 0);
    if (FAILED(hr)) {
      Debug::Print("LoadFromFile: LockRect failed %s\n", filePath.c_str());
      tex->Release();
      free(blocks);
      return NULL;
    }

    memcpy(locked.pBits, blocks, dataSize);
    tex->UnlockRect(0);
    free(blocks);

    return tex;
  }

  // Non-DXT (PNG, JPG for UI assets) -- use D3DX as before
  D3DTexture *tex = NULL;
  HRESULT hr = D3DXCreateTextureFromFileEx(
      Context::GetD3dDevice(), filePath.c_str(),
      D3DX_DEFAULT, D3DX_DEFAULT, 1, 0,
      D3DFMT_UNKNOWN, D3DPOOL_DEFAULT,
      D3DX_DEFAULT, D3DX_DEFAULT,
      0, NULL, NULL, &tex);
  if (FAILED(hr)) {
    Debug::Print("LoadFromFile: D3DX failed %s hr=0x%08X\n",
        filePath.c_str(), (unsigned int)hr);
    return NULL;
  }
  return tex;
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