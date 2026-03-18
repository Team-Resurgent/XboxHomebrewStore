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

// ---------------------------------------------------------------------------
// LoadFromFile
// DDS files are loaded via D3DXCreateTextureFromFileInMemoryEx.
// PNG/JPG (UI assets) fall back to D3DXCreateTextureFromFileEx.
// ---------------------------------------------------------------------------
D3DTexture *TextureHelper::LoadFromFile(const std::string filePath) {
  bool isDds = filePath.size() >= 4 &&
               filePath.substr(filePath.size() - 4) == ".dds";

  if (isDds) {
    FILE *f = fopen(filePath.c_str(), "rb");
    if (f == NULL) {
      Debug::Print("LoadFromFile: fopen failed %s\n", filePath.c_str());
      return NULL;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fileSize < 128) {
      fclose(f);
      DeleteFileA(filePath.c_str());
      Debug::Print("LoadFromFile: file too small %s\n", filePath.c_str());
      return NULL;
    }
    void *buf = malloc(fileSize);
    if (buf == NULL) {
      fclose(f);
      Debug::Print("LoadFromFile: malloc failed %s\n", filePath.c_str());
      return NULL;
    }
    fread(buf, fileSize, 1, f);
    fclose(f);
    D3DTexture *ddsTex = NULL;
    HRESULT ddsHr = D3DXCreateTextureFromFileInMemoryEx(
        Context::GetD3dDevice(),
        buf, (UINT)fileSize,
        D3DX_DEFAULT, D3DX_DEFAULT, 1,
        0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT,
        D3DX_DEFAULT, D3DX_DEFAULT,
        0, NULL, NULL, &ddsTex);
    free(buf);
    if (FAILED(ddsHr) || ddsTex == NULL) {
      Debug::Print("LoadFromFile: D3DX failed %s hr=0x%08X\n",
          filePath.c_str(), (unsigned int)ddsHr);
      DeleteFileA(filePath.c_str());
      return NULL;
    }
    return ddsTex;
  }

  // Non-DDS (PNG, JPG for UI assets)
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