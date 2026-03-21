#include "VersionScene.h"
#include "InstallPathScene.h"
#include "SceneManager.h"

#include "..\AppSettings.h"
#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Debug.h"
#include "..\FileSystem.h"
#include "..\Font.h"
#include "..\InputManager.h"
#include "..\Math.h"
#include "..\Network.h"
#include "..\StoreList.h"
#include "..\String.h"
#include "..\TextureCache.h"
#include "..\TextureHelper.h"
#include "..\UserState.h"
#include "..\ViewState.h"
#include "..\WebManager.h"

// ==========================================================================
// InstallPathScene callback
// ==========================================================================
static void OnInstallPathConfirmed(const std::string &path, void *userData) {
  VersionScene *scene = (VersionScene *)userData;
  if (scene != NULL) {
    scene->SetPendingInstallPath(path);
  }
}

// ==========================================================================
// Constructor / Destructor
// ==========================================================================
VersionScene::VersionScene(const StoreVersions &storeVersions) {
  mImageDownloader = new ImageDownloader();

  mStoreVersions = storeVersions;
  mSideBarFocused = true;
  mHighlightedVersionIndex = 0;
  mVersionIndex = 0;
  mListViewScrollOffset = 0.0f;
  mListViewContentHeight = 0.0f;
  mDescriptionHeight = 0.0f;
  mChangeLogHeight = 0.0f;
  mLastMeasuredVersionIndex = -1;

  mNeedsUpdate = false;
  mCoverQueued = false;
  mScreenshotQueued = false;
  mDownloading = false;
  mDownloadSucceeded = false;
  mDownloadCancelRequested = false;
  mDownloadCurrent = 0;
  mDownloadTotal = 0;
  mDownloadThread = NULL;
  mUnpacking = false;
  mUnpackCancelRequested = false;
  mUnpackCurrent = 0;
  mUnpackTotal = 0;
  mProgressIndex = 0;
  mProgressCount = 0;
  mShowFailedOverlay = false;
  mFailedMessage = "";
  mCheckingLinks = false;

  mWaitingForInstallPath = false;
  mShowAfterInstallDialog = false;

  // Text measurement deferred to first RenderListView to avoid hitch on load
}

VersionScene::~VersionScene() {
  // Cancel image downloads FIRST -- the download thread holds a raw pointer
  // to mStoreVersions.screenshot. If we destroy mStoreVersions while a
  // download is in flight the thread writes into freed memory.
  if (mImageDownloader != NULL) {
    mImageDownloader->CancelAll();
    delete mImageDownloader;
    mImageDownloader = NULL;
  }

  // cover is a VIEW pointer into TextureCache -- TextureCache owns the
  // texture lifetime. Never call Release() on it here or TextureCache
  // will hold a dangling pointer and crash StoreScene on resume.
  // mStoreVersions.cover = NULL; // just leave it, TextureCache manages it

  // screenshot IS owned by VersionScene (not in TextureCache)
  if (mStoreVersions.screenshot != NULL) {
    mStoreVersions.screenshot->Release();
    mStoreVersions.screenshot = NULL;
  }

  if (mDownloadThread != NULL) {
    mDownloadCancelRequested = true;
    WaitForSingleObject(mDownloadThread, INFINITE);
    CloseHandle(mDownloadThread);
    mDownloadThread = NULL;
  }
}

// ==========================================================================
// SetPendingInstallPath called by static callback from InstallPathScene
// ==========================================================================
void VersionScene::SetPendingInstallPath(const std::string &path) {
  mPendingInstallPath = path;
}

// ==========================================================================
// OnResume called when InstallPathScene pops back to us
// ==========================================================================
void VersionScene::OnResume() {
  mWaitingForInstallPath = false;

  if (!mPendingInstallPath.empty()) {
    StartDownload();
  }
  // If mPendingInstallPath is empty the user cancelled do nothing
}

// ==========================================================================
// BrowseInstallPath  push InstallPathScene pre-filled with last-used path
// ==========================================================================
void VersionScene::BrowseInstallPath() {
  if (mStoreVersions.versions.empty()) {
    return;
  }

  mWaitingForInstallPath = true;
  mPendingInstallPath = "";

  InstallPathScene *pathScene =
      new InstallPathScene(OnInstallPathConfirmed, this, "Select Install Path");
  Context::GetSceneManager()->PushScene(pathScene);
}

// ==========================================================================
// StartDownload  called after install path is confirmed
// ==========================================================================
void VersionScene::StartDownload() {
  if (mDownloading || mStoreVersions.versions.empty()) {
    return;
  }

  if (mDownloadThread != NULL) {
    WaitForSingleObject(mDownloadThread, INFINITE);
    CloseHandle(mDownloadThread);
    mDownloadThread = NULL;
  }

  StoreVersion *ver = &mStoreVersions.versions[mHighlightedVersionIndex];

  // Build download path from settings
  std::string downloadsRoot = AppSettings::GetDownloadPath();

  // Ensure the downloads root folder exists it may have been deleted
  bool rootExists = false;
  FileSystem::DirectoryExists(downloadsRoot, rootExists);
  if (!rootExists) {
    if (!FileSystem::DirectoryCreate(downloadsRoot)) {
      mShowFailedOverlay = true;
      return;
    }
  }

  mDownloadPath = FileSystem::CombinePath(downloadsRoot, ver->folderName);

  // Capture install path before clearing ? clear after so re-entry can't
  // double-fire
  mInstallPath = FileSystem::CombinePath(mPendingInstallPath, ver->folderName);
  mPendingInstallPath = "";

  mDownloadCancelRequested = false;
  mDownloadCurrent = 0;
  mDownloadTotal = 0;
  mProgressCount = ver->downloadFiles.size();
  mProgressIndex = (mProgressCount > 0) ? 1 : 0;
  mShowFailedOverlay = false;
  mShowAfterInstallDialog = false;
  mDownloading = true;

  mDownloadThread = CreateThread(NULL, 0, DownloadThreadProc, this, 0, NULL);
  if (mDownloadThread == NULL) {
    mDownloading = false;
  mDownloadSucceeded = false;
  }
}

// ==========================================================================
// HandleAfterInstall  called from thread after successful install
// ==========================================================================
void VersionScene::HandleAfterInstall() {
  AfterInstallAction action = AppSettings::GetAfterInstallAction();

  if (action == AfterInstallDelete) {
    FileSystem::DirectoryDelete(mDownloadPath, true);
  } else if (action == AfterInstallAsk) {
    mShowAfterInstallDialog = true;
  }
  // AfterInstallKeep: do nothing
}

// ==========================================================================
// Progress callbacks
// ==========================================================================
void VersionScene::DownloadProgressCb(uint32_t dlNow, uint32_t dlTotal,
    void *userData) {
  VersionScene *scene = (VersionScene *)userData;
  if (scene != NULL) {
    scene->mDownloadCurrent = dlNow;
    scene->mDownloadTotal = (uint32_t)dlTotal;
  }
}

bool VersionScene::UnpackProgressCb(int currentFile, int totalFiles,
    const char *currentFileName,
    void *userData) {
  (void)currentFileName;
  VersionScene *scene = (VersionScene *)userData;
  if (scene != NULL) {
    if (scene->mUnpackCancelRequested) {
      return false;
    }
    scene->mUnpackCurrent = currentFile;
    scene->mUnpackTotal = totalFiles;
  }
  return true;
}

// ==========================================================================
// Download thread
// ==========================================================================
static bool EndsWithZip(const std::string &path) {
  std::string ext = FileSystem::GetExtension(path);
  if (ext.size() != 4 || ext[0] != '.') {
    return false;
  }
  return (ext[1] == 'z' || ext[1] == 'Z') && (ext[2] == 'i' || ext[2] == 'I') &&
         (ext[3] == 'p' || ext[3] == 'P');
}

DWORD WINAPI VersionScene::DownloadThreadProc(LPVOID param) {
  VersionScene *scene = (VersionScene *)param;
  if (scene == NULL) {
    return 0;
  }

  StoreVersion *ver =
      &scene->mStoreVersions.versions[scene->mHighlightedVersionIndex];
  std::string versionId = ver->versionId;

  // Paths were set by StartDownload() on the main thread
  const std::string &downloadPath = scene->mDownloadPath;
  const std::string &installPath = scene->mInstallPath;

  FileSystem::DirectoryCreate(downloadPath);

  // Check all URLs are reachable before downloading
  scene->mCheckingLinks = true;
  for (size_t f = 0; f < ver->downloadFiles.size(); f++) {
    const std::string &entry = ver->downloadFiles[f];
    bool isUrl = (entry.size() >= 8 && entry.compare(0, 8, "https://") == 0) ||
                 (entry.size() >= 7 && entry.compare(0, 7, "http://") == 0);

    std::string checkUrl;
    if (isUrl) {
      checkUrl = entry;
    } else {
      // Store API file ? build the same URL TryDownloadVersionFile uses
      checkUrl = StoreList::GetActiveUrl() + "/api/Download/" + versionId +
                 String::Format("?fileIndex=%d", f);
    }

    if (!WebManager::TryCheckUrl(checkUrl)) {
      scene->mCheckingLinks = false;
      scene->mFailedMessage =
          "One or more download links are unavailable.\nPlease try again later "
          "or contact the store.";
      scene->mShowFailedOverlay = true;
      scene->mDownloading = false;
      scene->mNeedsUpdate = false;
      return 0;
    }
  }
  scene->mCheckingLinks = false;

  scene->mProgressIndex =
      1; // reset after checks ? counts from 1 of N for downloads

  std::vector<std::string> downloadedFileNames(ver->downloadFiles.size());
  bool downloadSucceeded = !ver->downloadFiles.empty();
  bool unpackSucceeded = true;

  for (size_t f = 0; downloadSucceeded && f < ver->downloadFiles.size(); f++) {
    if (scene->mDownloadCancelRequested) {
      downloadSucceeded = false;
      break;
    }

    scene->mProgressIndex = f + 1;
    scene->mDownloadCurrent = 0;
    scene->mDownloadTotal = 0;

    const std::string &entry = ver->downloadFiles[f];

    bool isUrl = (entry.size() >= 8 && entry.compare(0, 8, "https://") == 0) ||
                 (entry.size() >= 7 && entry.compare(0, 7, "http://") == 0);

    std::string fileName;

    if (isUrl) {
      size_t slash = entry.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < entry.size()) {
        std::string temp = entry.substr(slash + 1);
        size_t q = temp.find('?');
        if (q != std::string::npos) {
          temp = temp.substr(0, q);
        }
        if (temp.empty()) {
          temp = "download";
        }
        fileName = temp;
      } else {
        fileName = "download";
      }

      const char *illegal = "\\/:*?\"<>|";
      for (size_t i = 0; i < fileName.size(); ++i) {
        if (strchr(illegal, fileName[i]) != NULL) {
          fileName[i] = '_';
        }
      }

      downloadedFileNames[f] = fileName;

      std::string filePath = FileSystem::CombinePath(downloadPath, fileName);
      std::string actualName;

      downloadSucceeded = WebManager::TryDownloadWebData(
          entry, filePath, &actualName, DownloadProgressCb, scene,
          (volatile bool *)&scene->mDownloadCancelRequested);

      downloadedFileNames[f] = actualName.empty() ? fileName : actualName;
      if (!downloadSucceeded) {
        break;
      }
    } else {
      downloadedFileNames[f] = entry;

      std::string filePath = FileSystem::CombinePath(downloadPath, entry);

      downloadSucceeded = WebManager::TryDownloadVersionFile(
          versionId, (int32_t)f, filePath, DownloadProgressCb, scene,
          (volatile bool *)&scene->mDownloadCancelRequested);

      if (!downloadSucceeded) {
        break;
      }
    }
  }

  if (downloadSucceeded) {
    FileSystem::DirectoryCreate(installPath);

    scene->mUnpacking = true;
    scene->mUnpackCancelRequested = false;
    scene->mUnpackCurrent = 0;
    scene->mUnpackTotal = 0;
    scene->mProgressCount = downloadedFileNames.size();
    scene->mProgressIndex = 0;

    for (size_t f = 0; unpackSucceeded && f < downloadedFileNames.size(); f++) {
      const std::string &localName = downloadedFileNames[f];
      std::string srcPath = FileSystem::CombinePath(downloadPath, localName);

      scene->mProgressIndex = f + 1;

      if (EndsWithZip(localName)) {
        unpackSucceeded =
            xunzipFromFile(srcPath.c_str(), installPath.c_str(), true, true,
                false, UnpackProgressCb, scene);
      } else {
        std::string dstPath = FileSystem::CombinePath(installPath, localName);
        unpackSucceeded = FileSystem::FileCopy(srcPath, dstPath);
      }

      if (!unpackSucceeded) {
        break;
      }
    }

    scene->mUnpacking = false;
  }

  if (downloadSucceeded && unpackSucceeded) {
    // FIX: explicitly clear failed overlay and suppress re-fetch on success
    scene->mShowFailedOverlay = false;
    scene->mNeedsUpdate = false;
    UserState::TrySave(ver->appId, ver->versionId, &downloadPath, &installPath);
    scene->HandleAfterInstall();
  }

  scene->mDownloadSucceeded = downloadSucceeded;
  bool cancelled =
      scene->mDownloadCancelRequested || scene->mUnpackCancelRequested;
  bool failed = !downloadSucceeded || !unpackSucceeded;
  if (failed && !cancelled) {
    scene->mFailedMessage = "Download or install failed.\nCheck your network "
                            "and storage, then try again.";
    scene->mShowFailedOverlay = true;
  }

  // Only wipe folder on cancel (user backed out intentionally)
  if (cancelled) {
    FileSystem::DirectoryDelete(downloadPath, true);
  }

  scene->mDownloading = false;
  // FIX: only trigger a re-fetch on failure (so the UI can refresh state), not
  // on success
  scene->mNeedsUpdate = (failed && !cancelled);

  return 0;
}

// ==========================================================================
// Render
// ==========================================================================
void VersionScene::Render() {
  // FIX: don't re-fetch while a download is active ? avoids clobbering version
  // data mid-download
  if (mNeedsUpdate && !mDownloading) {
    mNeedsUpdate = false;
    // cover is a TextureCache view pointer -- never Release() it here
    mStoreVersions.cover = NULL;
    if (mStoreVersions.screenshot != NULL) {
      mStoreVersions.screenshot->Release();
      mStoreVersions.screenshot = NULL;
    }
    mCoverQueued = false;
    mScreenshotQueued = false;
    StoreManager::TryGetStoreVersions(mStoreVersions.appId, &mStoreVersions);
  }

  Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0.0f, 0.0f, (float)Context::GetScreenWidth(), (float)Context::GetScreenHeight());

  RenderHeader();
  RenderFooter();
  RenderVersionSidebar();
  RenderListView();

  if (mCheckingLinks) {
    RenderCheckingLinksOverlay();
  } else if (mDownloading) {
    RenderDownloadOverlay();
  }
  if (mShowFailedOverlay) {
    RenderFailedOverlay();
  }
  if (mShowAfterInstallDialog) {
    RenderAfterInstallDialog();
  }
}

// --------------------------------------------------------------------------
void VersionScene::RenderHeader() {
  float screenW = (float)Context::GetScreenWidth();

  Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0,
      screenW, ASSET_HEADER_HEIGHT);
  Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
  Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0xFFFFFFFF, 16, 12,
      ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);

  // Version beside title
  float titleW = 0.0f;
  Font::MeasureText(FONT_LARGE, "Xbox Homebrew Store", &titleW);
  Font::DrawText(FONT_NORMAL, APP_VERSION, COLOR_TEXT_GRAY,
      60.0f + titleW + 10.0f, 20.0f);

  // FTP IP top-right
  std::string ipStr = std::string("FTP: ") + Network::GetIP();
  float ipW = 0.0f;
  Font::MeasureText(FONT_NORMAL, ipStr.c_str(), &ipW);
  Font::DrawText(FONT_NORMAL, ipStr.c_str(), COLOR_TEXT_GRAY,
      screenW - ipW - 16.0f, 20.0f);
}

// --------------------------------------------------------------------------
void VersionScene::DrawFooterControl(float &x, float footerY,
    const char *iconName, const char *label) {
  float textWidth = 0.0f;
  D3DTexture *icon = TextureHelper::GetControllerIcon(iconName);
  if (icon != NULL) {
    Drawing::DrawTexturedRect(icon, 0xffffffff, x, footerY + 10.0f,
        ASSET_CONTROLLER_ICON_WIDTH,
        ASSET_CONTROLLER_ICON_HEIGHT);
    x += ASSET_CONTROLLER_ICON_WIDTH + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, label, COLOR_WHITE, x, footerY + 12.0f);
  Font::MeasureText(FONT_NORMAL, label, &textWidth);
  x += textWidth + 20.0f;
}

// --------------------------------------------------------------------------
void VersionScene::RenderFooter() {
  float footerY = (float)Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
  float x = 16.0f;

  Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0.0f,
      footerY, (float)Context::GetScreenWidth(),
      ASSET_FOOTER_HEIGHT);

  if (mCheckingLinks) {
    // no controls shown while link check is running
  } else if (mShowAfterInstallDialog) {
    DrawFooterControl(x, footerY, "ButtonA", "Delete");
    DrawFooterControl(x, footerY, "ButtonB", "Keep");
  } else if (mShowFailedOverlay) {
    DrawFooterControl(x, footerY, "ButtonA", "Close");
  } else if (mUnpacking || mDownloading) {
    DrawFooterControl(x, footerY, "ButtonB", "Cancel");
  } else {
    DrawFooterControl(x, footerY, "ButtonA", "Download");
    DrawFooterControl(x, footerY, "ButtonB", "Back");
    DrawFooterControl(x, footerY, "Dpad", "Move");
    DrawFooterControl(x, footerY, "ButtonWhite", "Refresh");
  }
}

// --------------------------------------------------------------------------
void VersionScene::RenderVersionSidebar() {
  float sidebarHeight = (float)(Context::GetScreenHeight() - ASSET_SIDEBAR_Y) -
                        ASSET_FOOTER_HEIGHT;
  Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0,
      ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH,
      sidebarHeight);

  Font::DrawText(FONT_NORMAL, "Versions...", COLOR_WHITE, 16,
      ASSET_SIDEBAR_Y + 16);

  int32_t versionCount = (int32_t)mStoreVersions.versions.size();
  int32_t maxItems = (int32_t)(sidebarHeight - 64) / 44;
  int32_t start = 0;
  if (versionCount >= maxItems) {
    start = Math::ClampInt32(mHighlightedVersionIndex - (maxItems / 2), 0,
        versionCount - maxItems);
  }

  int32_t itemCount = Math::MinInt32(start + maxItems, versionCount) - start;

  for (int32_t pass = 0; pass < 3; pass++) {
    float y = ASSET_SIDEBAR_Y + 64.0f;

    if (pass == 1) {
      Drawing::BeginStencil(16, ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH - 32.0f,
          sidebarHeight);
    }

    for (int32_t i = 0; i < itemCount; i++) {
      int32_t index = start + i;
      StoreVersion *storeVersion = &mStoreVersions.versions[index];
      bool highlighted = (index == mHighlightedVersionIndex);
      bool focused = mSideBarFocused && highlighted;

      if (pass == 0) {
        if (highlighted || focused) {
          Drawing::DrawTexturedRect(
              TextureHelper::GetCategoryHighlight(),
              focused ? COLOR_FOCUS_HIGHLIGHT : COLOR_HIGHLIGHT, 0.0f,
              y - 32.0f, ASSET_SIDEBAR_HIGHLIGHT_WIDTH,
              ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
        }
      } else if (pass == 1) {
        if (focused) {
          Font::DrawTextScrolling(
              FONT_NORMAL, storeVersion->version, COLOR_WHITE, 16.0f, y,
              ASSET_SIDEBAR_WIDTH - 64.0f, storeVersion->versionScrollState);
        } else {
          storeVersion->versionScrollState.active = false;
          Font::DrawText(FONT_NORMAL, storeVersion->version, COLOR_WHITE, 16,
              y);
        }
      } else {
        float itemTop = y - 24.0f;
        if (storeVersion->state == 1) {
          if (!focused) {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(),
                0x40ffffff, 0.0f, y - 32.0f,
                ASSET_SIDEBAR_HIGHLIGHT_WIDTH,
                ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
          }
          Drawing::DrawTexturedRect(
              TextureHelper::GetNewBadge(), 0xFFFFFFFF,
              ASSET_SIDEBAR_WIDTH + 8.0f - ASSET_BADGE_NEW_WIDTH, itemTop,
              ASSET_BADGE_NEW_WIDTH, ASSET_BADGE_NEW_HEIGHT);
        } else if (storeVersion->state == 2) {
          if (!focused) {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(),
                0x40ffffff, 0.0f, y - 32.0f,
                ASSET_SIDEBAR_HIGHLIGHT_WIDTH,
                ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
          }
          Drawing::DrawTexturedRect(
              TextureHelper::GetUpdateBadge(), 0xFFFFFFFF,
              ASSET_SIDEBAR_WIDTH + 8.0f - ASSET_BADGE_UPDATE_WIDTH, itemTop,
              ASSET_BADGE_UPDATE_WIDTH, ASSET_BADGE_UPDATE_HEIGHT);
        }
      }
      y += 44;
    }

    if (pass == 1) {
      Drawing::EndStencil();
    }
  }
}

// --------------------------------------------------------------------------
void VersionScene::RenderListView() {
  bool mTextureLoadedThisFrame = false;
  const float titleXPos = 200.0f;
  const float infoXPos = 350.0f;
  const float infoMaxWidth =
      (float)Context::GetScreenWidth() - infoXPos - 20.0f;

  StoreVersion *storeVersion =
      &mStoreVersions.versions[mHighlightedVersionIndex];

  if (mLastMeasuredVersionIndex == -1) {
    Font::MeasureTextWrapped(FONT_NORMAL, mStoreVersions.description,
        infoMaxWidth, NULL, &mDescriptionHeight);
  }
  if (mHighlightedVersionIndex != mLastMeasuredVersionIndex) {
    Font::MeasureTextWrapped(FONT_NORMAL, storeVersion->changeLog, infoMaxWidth,
        NULL, &mChangeLogHeight);
    mLastMeasuredVersionIndex = mHighlightedVersionIndex;
  }

  mListViewContentHeight = (float)ASSET_SCREENSHOT_HEIGHT + 16.0f + 30.0f +
                           30.0f + mDescriptionHeight + 8.0f + 30.0f * 5.0f +
                           mChangeLogHeight + 8.0f + 30.0f;

  float listVisibleHeight =
      (float)Context::GetScreenHeight() - ASSET_SIDEBAR_Y - ASSET_FOOTER_HEIGHT;
  float maxScroll = mListViewContentHeight - listVisibleHeight;
  if (maxScroll < 0.0f) {
    maxScroll = 0.0f;
  }
  if (mListViewScrollOffset > maxScroll) {
    mListViewScrollOffset = maxScroll;
  }
  if (mListViewScrollOffset < 0.0f) {
    mListViewScrollOffset = 0.0f;
  }

  float gridX = ASSET_SIDEBAR_WIDTH;
  float gridY = ASSET_SIDEBAR_Y - mListViewScrollOffset;

  Drawing::BeginStencil(ASSET_SIDEBAR_WIDTH, ASSET_SIDEBAR_Y,
      (float)Context::GetScreenWidth() - ASSET_SIDEBAR_WIDTH,
      listVisibleHeight);

  // Screenshot
  D3DTexture *screenshot = mStoreVersions.screenshot;
  if (screenshot == NULL) {
    if (ImageDownloader::IsScreenshotCached(mStoreVersions.appId)) {
      std::string ssPath = ImageDownloader::GetScreenshotCachePath(mStoreVersions.appId);
      bool ssDxt = ssPath.size() >= 4 && ssPath.substr(ssPath.size() - 4) == ".dxt";
      if (ssDxt && !mTextureLoadedThisFrame) {
        mStoreVersions.screenshot = TextureHelper::LoadFromFile(ssPath);
        screenshot = mStoreVersions.screenshot;
        mTextureLoadedThisFrame = true;
      }
      if (screenshot == NULL) screenshot = TextureHelper::GetScreenshot();
    } else {
      screenshot = TextureHelper::GetScreenshot();
      if (!mScreenshotQueued) {
        bool ssFailed = ImageDownloader::IsScreenshotFailed(mStoreVersions.appId);
        if (!ssFailed || AppSettings::GetRetryFailedOnView()) {
          if (ssFailed) ImageDownloader::ClearFailedScreenshot(mStoreVersions.appId);
          mImageDownloader->Queue(&mStoreVersions.screenshot, mStoreVersions.appId, IMAGE_SCREENSHOT);
        }
        mScreenshotQueued = true;
      }
    }
  }
  Drawing::DrawTexturedRect(screenshot, 0xFFFFFFFF, titleXPos, gridY,
      ASSET_SCREENSHOT_WIDTH, ASSET_SCREENSHOT_HEIGHT);

  // Cover
  D3DTexture *cover = mStoreVersions.cover;
  if (cover == NULL) {
    cover = TextureCache::Get(mStoreVersions.appId);
    if (cover != NULL) {
      mStoreVersions.cover = cover;
    } else if (ImageDownloader::IsCoverCached(mStoreVersions.appId)) {
      std::string cvPath = ImageDownloader::GetCoverCachePath(mStoreVersions.appId);
      bool cvDxt = cvPath.size() >= 4 && cvPath.substr(cvPath.size() - 4) == ".dxt";
      if (cvDxt && !mTextureLoadedThisFrame) {
        D3DTexture *loaded = TextureHelper::LoadFromFile(cvPath);
        if (loaded != NULL) {
          TextureCache::Put(mStoreVersions.appId, loaded);
          mStoreVersions.cover = loaded;
          cover = loaded;
          mTextureLoadedThisFrame = true;
        } else {
          cover = TextureHelper::GetCover();
        }
      } else {
        cover = TextureHelper::GetCover();
      }
    } else {
      cover = TextureHelper::GetCover();
      if (!mCoverQueued) {
        bool coverFailed = ImageDownloader::IsCoverFailed(mStoreVersions.appId);
        if (!coverFailed || AppSettings::GetRetryFailedOnView()) {
          if (coverFailed) ImageDownloader::ClearFailedCover(mStoreVersions.appId);
          mImageDownloader->Queue(&mStoreVersions.cover, mStoreVersions.appId, IMAGE_COVER);
        }
        mCoverQueued = true;
      }
    }
  }
  Drawing::DrawTexturedRect(cover, 0xFFFFFFFF, 216 + ASSET_SCREENSHOT_WIDTH,
      gridY, 144, 204);

  gridY += ASSET_SCREENSHOT_HEIGHT + 16;

  Font::DrawText(FONT_NORMAL, "Name:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawText(FONT_NORMAL, mStoreVersions.name, COLOR_TEXT_GRAY, infoXPos,
      gridY);
  gridY += 30;
  Font::DrawText(FONT_NORMAL, "Author:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawText(FONT_NORMAL, mStoreVersions.author, COLOR_TEXT_GRAY, infoXPos,
      gridY);
  gridY += 30;
  Font::DrawText(FONT_NORMAL, "Description:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawTextWrapped(FONT_NORMAL, mStoreVersions.description,
      COLOR_TEXT_GRAY, infoXPos, gridY, infoMaxWidth);
  gridY += mDescriptionHeight + 8.0f;

  Font::DrawText(FONT_NORMAL, "Version:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawText(FONT_NORMAL, storeVersion->version, COLOR_TEXT_GRAY, infoXPos,
      gridY);
  gridY += 30;
  Font::DrawText(FONT_NORMAL, "Title ID:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawText(FONT_NORMAL, storeVersion->titleId, COLOR_TEXT_GRAY, infoXPos,
      gridY);
  gridY += 30;
  Font::DrawText(FONT_NORMAL, "Release Date:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawText(FONT_NORMAL, storeVersion->releaseDate, COLOR_TEXT_GRAY,
      infoXPos, gridY);
  gridY += 30;
  Font::DrawText(FONT_NORMAL, "Region:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawText(FONT_NORMAL, storeVersion->region, COLOR_TEXT_GRAY, infoXPos,
      gridY);
  gridY += 30;
  Font::DrawText(FONT_NORMAL, "Change Log:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawTextWrapped(FONT_NORMAL, storeVersion->changeLog, COLOR_TEXT_GRAY,
      infoXPos, gridY, infoMaxWidth);
  gridY += mChangeLogHeight + 8.0f;
  Font::DrawText(FONT_NORMAL, "Size:", COLOR_WHITE, titleXPos, gridY);
  Font::DrawText(FONT_NORMAL, String::FormatSize(storeVersion->size),
      COLOR_TEXT_GRAY, infoXPos, gridY);

  Drawing::EndStencil();
}

// --------------------------------------------------------------------------
void VersionScene::RenderCheckingLinksOverlay() {
  const float w = (float)Context::GetScreenWidth();
  const float h = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, w, h);

  const float panelW = 320.0f;
  const float panelH = 70.0f;
  float px = (w - panelW) * 0.5f;
  float py = (h - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 4.0f);

  std::string msg =
      mProgressCount > 0
          ? String::Format("Verifying %d download links...", mProgressCount)
          : "Verifying download links...";
  Font::DrawText(FONT_NORMAL, msg, COLOR_WHITE, px + 20.0f, py + 22.0f);
}

// --------------------------------------------------------------------------
void VersionScene::RenderDownloadOverlay() {
  const float w = (float)Context::GetScreenWidth();
  const float h = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, w, h);

  const float panelW = 400.0f;
  const float panelH = 140.0f;
  float px = (w - panelW) * 0.5f;
  float py = (h - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);

  const float barY = py + 50.0f;
  const float barH = 24.0f;
  const float barMargin = 20.0f;
  float barW = panelW - barMargin * 2.0f;
  float barX = px + barMargin;

  if (mUnpacking) {
    std::string title = (mProgressCount > 0)
                            ? String::Format("Unpacking %d of %d...",
                                  mProgressIndex, mProgressCount)
                            : "Unpacking...";
    Font::DrawText(FONT_NORMAL, title, COLOR_WHITE, px + 20.0f, py + 16.0f);
    Drawing::DrawFilledRect(COLOR_SECONDARY, barX, barY, barW, barH);
    int total = mUnpackTotal;
    if (total > 0) {
      float pct = (float)mUnpackCurrent / (float)total;
      if (pct > 1.0f) {
        pct = 1.0f;
      }
      Drawing::DrawFilledRect(COLOR_DOWNLOAD, barX, barY, barW * pct, barH);
    }
    std::string prog =
        total > 0 ? String::Format("%d / %d files", mUnpackCurrent, total)
                  : "Preparing...";
    Font::DrawText(FONT_NORMAL, prog, COLOR_TEXT_GRAY, px + 20.0f,
        barY + barH + 8.0f);
  } else {
    std::string title = (mProgressCount > 0)
                            ? String::Format("Downloading %d of %d...",
                                  mProgressIndex, mProgressCount)
                            : "Downloading...";
    Font::DrawText(FONT_NORMAL, title, COLOR_WHITE, px + 20.0f, py + 16.0f);
    Drawing::DrawFilledRect(COLOR_SECONDARY, barX, barY, barW, barH);
    uint32_t total = mDownloadTotal;
    if (total > 0) {
      float pct = (float)mDownloadCurrent / (float)total;
      if (pct > 1.0f) {
        pct = 1.0f;
      }
      Drawing::DrawFilledRect(COLOR_DOWNLOAD, barX, barY, barW * pct, barH);
    }
    std::string prog =
        total > 0 ? String::Format("%s / %s",
                        String::FormatSize(mDownloadCurrent).c_str(),
                        String::FormatSize(total).c_str())
                  : "Connecting...";
    Font::DrawText(FONT_NORMAL, prog, COLOR_TEXT_GRAY, px + 20.0f,
        barY + barH + 8.0f);
  }

  // Cancel hint ? centred
  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float lblW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Cancel", &lblW);
  float pairW = iW + 4.0f + lblW;
  float hx = px + (panelW - pairW) * 0.5f;
  float hy = py + panelH - 28.0f;
  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB != NULL) {
    Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy - 2.0f, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Cancel", COLOR_WHITE, hx, hy);
}

// --------------------------------------------------------------------------
void VersionScene::RenderFailedOverlay() {
  const float w = (float)Context::GetScreenWidth();
  const float h = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, w, h);

  const float panelW = 440.0f;
  const float panelH = 120.0f;
  float px = (w - panelW) * 0.5f;
  float py = (h - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(0xFFEF5350, px, py, panelW, 4.0f);

  float msgMaxWidth = panelW - 40.0f;
  Font::DrawTextWrapped(FONT_NORMAL, mFailedMessage, COLOR_WHITE, px + 20.0f,
      py + 18.0f, msgMaxWidth);

  // Close button ? centred
  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float lblW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Close", &lblW);
  float pairW = iW + 4.0f + lblW;
  float hx = px + (panelW - pairW) * 0.5f;
  float hy = py + panelH - 32.0f;
  D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
  if (iconA != NULL) {
    Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy - 2.0f, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Close", COLOR_TEXT_GRAY, hx, hy);
}

// --------------------------------------------------------------------------
void VersionScene::RenderAfterInstallDialog() {
  const float w = (float)Context::GetScreenWidth();
  const float h = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, w, h);

  const float panelW = 460.0f;
  const float panelH = 120.0f;
  float px = (w - panelW) * 0.5f;
  float py = (h - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 4.0f);

  Font::DrawText(FONT_NORMAL, "Install complete!", COLOR_WHITE, px + 16.0f,
      py + 16.0f);
  Font::DrawText(FONT_NORMAL, "Keep the downloaded zip file?", COLOR_TEXT_GRAY,
      px + 16.0f, py + 42.0f);

  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float delW = 0.0f;
  float kepW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Delete", &delW);
  Font::MeasureText(FONT_NORMAL, "Keep", &kepW);
  float pairW = (iW + 4.0f + delW) + 24.0f + (iW + 4.0f + kepW);
  float hx = px + (panelW - pairW) * 0.5f;
  float hy = py + panelH - 32.0f;

  D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
  if (iconA != NULL) {
    Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy - 2.0f, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Delete", 0xFFEF5350, hx, hy);
  hx += delW + 24.0f;

  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB != NULL) {
    Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy - 2.0f, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Keep", COLOR_WHITE, hx, hy);
}

// ==========================================================================
// Update
// ==========================================================================
// RefreshCoverArt -- deletes cached cover and screenshot for this app,
// removes from TextureCache, clears fail markers, queues fresh downloads.
// ==========================================================================
void VersionScene::RefreshCoverArt() {
  std::string appId = mStoreVersions.appId;
  if (appId.empty()) return;

  // Cancel any in-flight downloads first so we dont race with the delete
  mImageDownloader->CancelAll();

  // Delete cached .dxt files from disk
  std::string coverPath = ImageDownloader::GetCoverCachePath(appId);
  std::string ssPath    = ImageDownloader::GetScreenshotCachePath(appId);
  DeleteFileA(coverPath.c_str());
  DeleteFileA(ssPath.c_str());

  // Also delete .jpg if converter hasnt finished yet
  std::string coverJpg = coverPath.substr(0, coverPath.size() - 4) + ".jpg";
  std::string ssJpg    = ssPath.substr(0, ssPath.size() - 4) + ".jpg";
  DeleteFileA(coverJpg.c_str());
  DeleteFileA(ssJpg.c_str());

  // Clear fail markers so they retry
  ImageDownloader::ClearFailedCover(appId);
  ImageDownloader::ClearFailedScreenshot(appId);

  // Clear TextureCache and null all StoreScene cover pointers safely
  StoreManager::InvalidateCovers();
  mStoreVersions.cover = NULL;
  if (mStoreVersions.screenshot != NULL) {
    mStoreVersions.screenshot->Release();
    mStoreVersions.screenshot = NULL;
  }

  // Reset queue flags so RenderListView re-queues both downloads
  mCoverQueued = false;
  mScreenshotQueued = false;

  // Re-fetch app info so description/versions are up to date
  mDescriptionHeight = 0.0f;
  mChangeLogHeight = 0.0f;
  mLastMeasuredVersionIndex = -1;
  StoreManager::TryGetStoreVersions(appId, &mStoreVersions);

  // Rescan cover count since we deleted one
  ImageDownloader::ResetCachedCoverCount();

  Debug::Print("VersionScene: RefreshCoverArt for %s\n", appId.c_str());
}

// ==========================================================================
void VersionScene::Update() {
  // ---- After-install dialog ----
  if (mShowAfterInstallDialog) {
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      FileSystem::DirectoryDelete(mDownloadPath, true);
      mShowAfterInstallDialog = false;
    } else if (InputManager::ControllerPressed(ControllerB, -1)) {
      // Keep do nothing
      mShowAfterInstallDialog = false;
    }
    return;
  }

  // ---- Failed overlay ----
  if (mShowFailedOverlay) {
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      mShowFailedOverlay = false;
      // Only wipe download folder if download failed (not unzip failure)
      // so the zip can be reused on retry without re-downloading
      if (!mDownloadSucceeded) {
        FileSystem::DirectoryDelete(mDownloadPath, true);
      }
    }
    return;
  }

  // ---- Download / unpack in progress ----
  if (mDownloading) {
    if (InputManager::ControllerPressed(ControllerB, -1)) {
      if (mUnpacking) {
        mUnpackCancelRequested = true;
      } else {
        mDownloadCancelRequested = true;
      }
    }
    return;
  }

  // ---- Waiting for path browser to return block all input ----
  if (mWaitingForInstallPath) {
    return;
  }

  // ---- Normal browse mode ----
  const float listScrollStep = 44.0f;
  float listVisibleHeight =
      (float)Context::GetScreenHeight() - ASSET_SIDEBAR_Y - ASSET_FOOTER_HEIGHT;
  float listMaxScroll = mListViewContentHeight - listVisibleHeight;
  if (listMaxScroll < 0.0f) {
    listMaxScroll = 0.0f;
  }

  if (mSideBarFocused) {
    if (InputManager::ControllerPressed(ControllerDpadUp, -1)) {
      mHighlightedVersionIndex =
          mHighlightedVersionIndex > 0
              ? mHighlightedVersionIndex - 1
              : (int32_t)mStoreVersions.versions.size() - 1;
    } else if (InputManager::ControllerPressed(ControllerDpadDown, -1)) {
      mHighlightedVersionIndex =
          mHighlightedVersionIndex < (int32_t)mStoreVersions.versions.size() - 1
              ? mHighlightedVersionIndex + 1
              : 0;
    } else if (InputManager::ControllerPressed(ControllerA, -1)) {
      BrowseInstallPath();
    } else if (InputManager::ControllerPressed(ControllerB, -1)) {
      Context::GetSceneManager()->PopScene();
    } else if (InputManager::ControllerPressed(ControllerWhite, -1)) {
      RefreshCoverArt();
    }

    ControllerState state;
    if (InputManager::TryGetControllerState(-1, &state) &&
        listMaxScroll > 0.0f) {
      mListViewScrollOffset -= state.ThumbRightDy;
      if (mListViewScrollOffset < 0.0f) {
        mListViewScrollOffset = 0.0f;
      }
      if (mListViewScrollOffset > listMaxScroll) {
        mListViewScrollOffset = listMaxScroll;
      }
    }
  }
}