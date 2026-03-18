#include "LoadingScene.h"
#include "SceneManager.h"
#include "StoreScene.h"

#include "..\AppSettings.h"
#include "..\Context.h"
#include "..\Debug.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\DriveMount.h"
#include "..\FileSystem.h"
#include "..\Font.h"
#include "..\FtpServer.h"
#include "..\InputManager.h"
#include "..\Main.h"
#include "..\Network.h"
#include "..\StoreList.h"
#include "..\StoreManager.h"
#include "..\String.h"
#include "..\TextureHelper.h"
#include "..\UserState.h"
#include "..\WebManager.h"

#define TOTAL_STEPS 10


static void UpdateDownloadProgressCb(uint32_t dlNow, uint32_t dlTotal,
    void *userData) {
  LoadingScene *scene = (LoadingScene *)userData;
  if (scene != NULL) {
    scene->mUpdateDlNow = dlNow;
    scene->mUpdateDlTotal = dlTotal;
  }
}

static DWORD WINAPI UpdateThreadProc(LPVOID param) {
  LoadingScene *scene = (LoadingScene *)param;
  if (scene == NULL) {
    return 0;
  }

  Debug::Print("UpdateThread: downloading %s\n", scene->mUpdateZipUrl.c_str());

  FileSystem::DirectoryCreate("T:\\XHSUpdate");
  std::string zipPath = "T:\\XHSUpdate\\update.zip";
  DeleteFileA(zipPath.c_str());

  scene->mUpdateStatus = LoadingScene::UPDATE_DOWNLOADING;

  std::string actualName;
  bool ok = WebManager::TryDownloadWebData(
      scene->mUpdateZipUrl, zipPath, &actualName, UpdateDownloadProgressCb,
      scene, (volatile bool *)&scene->mUpdateCancelRequested);

  if (!ok || scene->mUpdateCancelRequested) {
    Debug::Print("UpdateThread: download failed or cancelled\n");
    scene->mUpdateStatus = scene->mUpdateCancelRequested
                               ? LoadingScene::UPDATE_IDLE
                               : LoadingScene::UPDATE_FAILED;
    return 0;
  }

  if (!actualName.empty()) {
    zipPath = "T:\\XHSUpdate\\" + actualName;
  }

  // Verify the zip file actually exists and has size
  {
    HANDLE hTest =
        CreateFileA(zipPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hTest == INVALID_HANDLE_VALUE) {
      Debug::Print("UpdateThread: zip not found at %s (err %lu)\n",
          zipPath.c_str(), GetLastError());
      scene->mUpdateStatus = LoadingScene::UPDATE_FAILED;
      return 0;
    }
    DWORD fileSzHi = 0;
    DWORD fileSzLo = GetFileSize(hTest, &fileSzHi);
    CloseHandle(hTest);
    Debug::Print("UpdateThread: zip path=%s size=%lu bytes\n", zipPath.c_str(),
        fileSzLo);
  }

  Debug::Print("UpdateThread: unpacking %s -> D:\\\n", zipPath.c_str());
  scene->mUpdateStatus = LoadingScene::UPDATE_UNPACKING;

  bool unzipOk =
      xunzipFromFile(zipPath.c_str(), "D:\\", true, true, false, NULL, NULL);

  Debug::Print("UpdateThread: xunzipFromFile returned %s\n",
      unzipOk ? "true" : "false");

  if (!unzipOk) {
    Debug::Print("UpdateThread: unzip failed\n");
    scene->mUpdateStatus = LoadingScene::UPDATE_FAILED;
    return 0;
  }

  // Clean up all temp files
  FileSystem::DirectoryDelete("T:\\XHSUpdate", true);

  Debug::Print("UpdateThread: done, signalling reboot\n");
  scene->mUpdateStatus = LoadingScene::UPDATE_REBOOTING;
  return 0;
}

LoadingScene::LoadingScene() : mProgress(0) {
  mUpdateAvailable = false;
  mUpdatePromptShown = false;
  mUpdateStatus = UPDATE_IDLE;
  mUpdateCancelRequested = false;
  mUpdateDlNow = 0;
  mUpdateDlTotal = 0;
  mUpdateThread = NULL;
}

LoadingScene::~LoadingScene() {
  if (mUpdateThread != NULL) {
    mUpdateCancelRequested = true;
    WaitForSingleObject(mUpdateThread, INFINITE);
    CloseHandle(mUpdateThread);
    mUpdateThread = NULL;
  }
}

void LoadingScene::LaunchUpdate() {
  mUpdateCancelRequested = false;
  mUpdateDlNow = 0;
  mUpdateDlTotal = 0;
  mUpdateStatus = UPDATE_DOWNLOADING;
  mUpdateThread = CreateThread(NULL, 0, UpdateThreadProc, this, 0, NULL);
  if (mUpdateThread == NULL) {
    mUpdateStatus = UPDATE_FAILED;
  }
}

void LoadingScene::ProceedToStore() {
  StoreList::EnsureCacheDirs(); // creates T:\Cache\<storeCRC>\Covers|Screenshots|Meta
  FileSystem::DirectoryCreate("HDD0-E:\\Homebrew");
  FileSystem::DirectoryCreate("HDD0-E:\\Homebrew\\Downloads");
  FileSystem::DirectoryCreate("HDD0-E:\\Homebrew\\Installs");


  SceneManager *sceneManager = Context::GetSceneManager();
  sceneManager->PopScene();
  sceneManager->PushScene(new StoreScene());
}

void LoadingScene::Update() {
  // ---- Update downloading / unpacking ----
  if (mUpdateStatus == UPDATE_DOWNLOADING ||
      mUpdateStatus == UPDATE_UNPACKING) {
    return;
  }

  // ---- Update complete � reboot into new XBE ----
  if (mUpdateStatus == UPDATE_REBOOTING) {
    if (mUpdateThread != NULL) {
      WaitForSingleObject(mUpdateThread, INFINITE);
      CloseHandle(mUpdateThread);
      mUpdateThread = NULL;
    }
    Debug::Print("LoadingScene: launching updated XBE\n");
    // XLaunchNewImage uses D:\ not HDD0-D:\, and needs a valid LAUNCH_DATA
    // struct
    LAUNCH_DATA launchData;
    memset(&launchData, 0, sizeof(launchData));
    XLaunchNewImage("D:\\XboxHomebrewStore.xbe", &launchData);
    // Should never reach here � if it does, path was wrong or XBE missing
    Debug::Print("LoadingScene: XLaunchNewImage returned! err=%lu falling back "
                 "to store\n",
        GetLastError());
    ProceedToStore(); // fallback if launch fails
    return;
  }

  // ---- Update failed ----
  if (mUpdateStatus == UPDATE_FAILED) {
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      mUpdateStatus = UPDATE_IDLE;
      ProceedToStore();
    }
    return;
  }

  // ---- Update prompt ----
  if (mUpdateAvailable && mUpdatePromptShown) {
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      LaunchUpdate();
    } else if (InputManager::ControllerPressed(ControllerB, -1)) {
      mUpdateAvailable = false;
      mUpdatePromptShown = false;
      // Do NOT call ProceedToStore() -- mProgress is already at 8
      // so let the normal loop finish StoreManager::Init() and FtpServer::Init()
    }
    return;
  }

  // ---- Normal loading steps ----
  if (mProgress >= TOTAL_STEPS) {
    ProceedToStore();
    return;
  }

  switch (mProgress) {
  case 0:
    DriveMount::Init();
    break;
  case 1:
    Network::Init();
    break;
  case 2:
    WebManager::Init();
    break;
  case 3:
    WebManager::TrySyncTime();
    break;
  case 4:
    if (!AppSettings::Load()) {
      AppSettings::Save();
    }
    break;
  case 5:
    if (!StoreList::Load()) {
      StoreList::Save();
    }
    break;
  case 6:
    TextureHelper::Init();
    break;
  case 7: {
    // Version check runs BEFORE StoreManager::Init so it completes before
    // MetaCache starts its background fetch -- avoids shared curl collision
    std::string latestTag;
    std::string latestZipUrl;
    if (WebManager::TryGetLatestRelease(latestTag, latestZipUrl)) {
      if (String::IsNewerVersion(APP_VERSION, latestTag)) {
        Debug::Print("Update available: %s -> %s\n", APP_VERSION,
            latestTag.c_str());
        mUpdateTag = latestTag;
        mUpdateZipUrl = latestZipUrl;
        mUpdateAvailable = true;
        mUpdatePromptShown = true;
        mProgress++;
        return;
      }
    }
    Debug::Print("No update (current: %s latest: %s)\n", APP_VERSION,
        latestTag.c_str());
    break;
  }
  case 8:
    StoreList::EnsureCacheDirs(); // must exist before MetaCache starts writing
    StoreManager::Init();
    break;
  case 9:
    FtpServer::Init();
    break;
  default:
    break;
  }
  mProgress++;
}

void LoadingScene::Render() {
  float w = (float)Context::GetScreenWidth();
  float h = (float)Context::GetScreenHeight();
  Drawing::DrawFilledRect(COLOR_BG, 0, 0, w, h);

  float cx = w * 0.5f;
  float cy = h * 0.5f;

  // ---- Update prompt ----
  if (mUpdateAvailable && mUpdatePromptShown && mUpdateStatus == UPDATE_IDLE) {
    float panelW = 500.0f;
    float panelH = 160.0f;
    float px = (w - panelW) * 0.5f;
    float py = (h - panelH) * 0.5f;

    Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
    Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 4.0f);

    std::string title =
        String::Format("Update available: %s", mUpdateTag.c_str());
    std::string sub = String::Format("Current version: %s", APP_VERSION);
    std::string sub2 = "Download and install now?";
    float tw = 0.0f;
    float sw = 0.0f;
    float s2w = 0.0f;
    Font::MeasureText(FONT_NORMAL, title, &tw);
    Font::MeasureText(FONT_NORMAL, sub, &sw);
    Font::MeasureText(FONT_NORMAL, sub2, &s2w);
    Font::DrawText(FONT_NORMAL, title, COLOR_WHITE, cx - tw * 0.5f, py + 18.0f);
    Font::DrawText(FONT_NORMAL, sub, COLOR_TEXT_GRAY, cx - sw * 0.5f,
        py + 46.0f);
    Font::DrawText(FONT_NORMAL, sub2, COLOR_TEXT_GRAY, cx - s2w * 0.5f,
        py + 70.0f);

    float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
    float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
    float updW = 0.0f;
    float skpW = 0.0f;
    Font::MeasureText(FONT_NORMAL, "Update", &updW);
    Font::MeasureText(FONT_NORMAL, "Skip", &skpW);
    float pairW = (iW + 4.0f + updW) + 32.0f + (iW + 4.0f + skpW);
    float hx = cx - pairW * 0.5f;
    float hy = py + panelH - 38.0f;

    D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
    if (iconA != NULL) {
      Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy - 2.0f, iW, iH);
      hx += iW + 4.0f;
    }
    Font::DrawText(FONT_NORMAL, "Update", COLOR_WHITE, hx, hy);
    hx += updW + 32.0f;

    D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
    if (iconB != NULL) {
      Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy - 2.0f, iW, iH);
      hx += iW + 4.0f;
    }
    Font::DrawText(FONT_NORMAL, "Skip", COLOR_TEXT_GRAY, hx, hy);
    return;
  }

  // ---- Downloading / unpacking update ----
  if (mUpdateStatus == UPDATE_DOWNLOADING ||
      mUpdateStatus == UPDATE_UNPACKING) {
    float panelW = 460.0f;
    float panelH = 120.0f;
    float px = (w - panelW) * 0.5f;
    float py = (h - panelH) * 0.5f;

    Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
    Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 4.0f);

    const char *title = (mUpdateStatus == UPDATE_UNPACKING)
                            ? "Installing update..."
                            : "Downloading update...";
    float tw = 0.0f;
    Font::MeasureText(FONT_NORMAL, title, &tw);
    Font::DrawText(FONT_NORMAL, title, COLOR_WHITE, cx - tw * 0.5f, py + 18.0f);

    float barX = px + 20.0f;
    float barY = py + 52.0f;
    float barW = panelW - 40.0f;
    float barH = 20.0f;
    Drawing::DrawFilledRect(COLOR_SECONDARY, barX, barY, barW, barH);

    if (mUpdateStatus == UPDATE_DOWNLOADING && mUpdateDlTotal > 0) {
      float pct = (float)mUpdateDlNow / (float)mUpdateDlTotal;
      if (pct > 1.0f) {
        pct = 1.0f;
      }
      Drawing::DrawFilledRect(COLOR_DOWNLOAD, barX, barY, barW * pct, barH);
      std::string prog =
          String::Format("%s / %s", String::FormatSize(mUpdateDlNow).c_str(),
              String::FormatSize(mUpdateDlTotal).c_str());
      float pw = 0.0f;
      Font::MeasureText(FONT_NORMAL, prog, &pw);
      Font::DrawText(FONT_NORMAL, prog, COLOR_TEXT_GRAY, cx - pw * 0.5f,
          barY + barH + 8.0f);
    } else {
      Drawing::DrawFilledRect(COLOR_DOWNLOAD, barX, barY, barW, barH);
    }
    return;
  }

  // ---- Update failed ----
  if (mUpdateStatus == UPDATE_FAILED) {
    float panelW = 420.0f;
    float panelH = 100.0f;
    float px = (w - panelW) * 0.5f;
    float py = (h - panelH) * 0.5f;

    Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
    Drawing::DrawFilledRect(0xFFEF5350, px, py, panelW, 4.0f);

    std::string msg = "Update failed. Continuing to store.";
    float mw = 0.0f;
    Font::MeasureText(FONT_NORMAL, msg, &mw);
    Font::DrawText(FONT_NORMAL, msg, COLOR_WHITE, cx - mw * 0.5f, py + 22.0f);

    float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
    float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
    float cW = 0.0f;
    Font::MeasureText(FONT_NORMAL, "Continue", &cW);
    float hx = cx - (iW + 4.0f + cW) * 0.5f;
    float hy = py + panelH - 32.0f;
    D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
    if (iconA != NULL) {
      Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy - 2.0f, iW, iH);
      hx += iW + 4.0f;
    }
    Font::DrawText(FONT_NORMAL, "Continue", COLOR_TEXT_GRAY, hx, hy);
    return;
  }

  // ---- Normal loading ----
  int step = (mProgress < TOTAL_STEPS) ? (mProgress + 1) : TOTAL_STEPS;
  std::string progressText = String::Format("%d of %d", step, TOTAL_STEPS);
  float loadingW = 0.0f;
  float progressW = 0.0f;
  Font::MeasureText(FONT_LARGE, "Loading...", &loadingW);
  Font::MeasureText(FONT_LARGE, progressText, &progressW);
  Font::DrawText(FONT_LARGE, "Loading...", COLOR_WHITE, cx - loadingW * 0.5f,
      cy - 40.0f);
  Font::DrawText(FONT_LARGE, progressText, COLOR_WHITE, cx - progressW * 0.5f,
      cy);
}