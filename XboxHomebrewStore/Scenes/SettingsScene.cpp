#include "SettingsScene.h"
#include "InstallPathScene.h"
#include "SceneManager.h"
#include "StoreManagerScene.h"

#include "..\AppSettings.h"
#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\FileSystem.h"
#include "..\Debug.h"
#include "..\Font.h"
#include "..\ImageDownloader.h"
#include "..\InputManager.h"
#include "..\StoreList.h"
#include "..\StoreManager.h"
#include "..\String.h"
#include "..\TextureHelper.h"

// ==========================================================================
// Path browser callback
// ==========================================================================
static void OnDownloadPathConfirmed(const std::string &path, void *userData) {
  SettingsScene *self = (SettingsScene *)userData;
  if (self != NULL) {
    self->SetDownloadPath(path);
  }
}

static void OnCachePathConfirmed(const std::string &path, void *userData) {
  SettingsScene *self = (SettingsScene *)userData;
  if (self != NULL) {
    self->SetCachePath(path);
  }
}

// ==========================================================================
// Layout constants
// ==========================================================================
static const float BODY_TOP = ASSET_SIDEBAR_Y + 16.0f;
static const float ROW_H = 72.0f;
static const float ROW_MARGIN = 24.0f;
static const float SEC_GAP = 28.0f;    // gap above each section header
static const float SEC_HEAD_H = 26.0f; // section label height

// ==========================================================================
// Constructor / Destructor
// ==========================================================================
SettingsScene::SettingsScene()
    : mSelectedRow(0), mPickerOpen(false), mPickerSel(0),
      mCachePickerOpen(false), mCachePickerSel(0),
      mClearCacheConfirmOpen(false), mClearCacheDone(false),
  mClearFailedConfirmOpen(false), mClearFailedDone(false),
      mScrollOffset(0.0f) {
  mDownloadPath = AppSettings::GetDownloadPath();
  mAfterInstallAction = AppSettings::GetAfterInstallAction();
  mShowCachePartitions = AppSettings::GetShowCachePartitions();
  mPreCacheOnIdle = AppSettings::GetPreCacheOnIdle();
  mRetryFailedOnView = AppSettings::GetRetryFailedOnView();
  mCacheLocation = AppSettings::GetCacheLocation();
  mCachePath = AppSettings::GetCachePath();
}

SettingsScene::~SettingsScene() {}

void SettingsScene::OnResume() {
  // Refresh store name in case StoreManagerScene changed the active store
}

// ==========================================================================
// SaveAndPop
// ==========================================================================
void SettingsScene::SaveAndPop() {
  AppSettings::SetDownloadPath(mDownloadPath);
  AppSettings::SetAfterInstallAction(mAfterInstallAction);
  AppSettings::SetShowCachePartitions(mShowCachePartitions);
  AppSettings::SetPreCacheOnIdle(mPreCacheOnIdle);
  AppSettings::SetRetryFailedOnView(mRetryFailedOnView);

  // If cache location or path is changing, capture the old meta dir BEFORE
  // the setting changes -- then purge it so stale files don't accumulate
  bool cacheChanging =
      (mCacheLocation != AppSettings::GetCacheLocation()) ||
      (mCachePath != AppSettings::GetCachePath());
  std::string oldMetaDir;
  if (cacheChanging) {
    oldMetaDir = StoreList::GetActiveCacheRoot() + "\\Meta";
  }

  AppSettings::SetCacheLocation(mCacheLocation);
  AppSettings::SetCachePath(mCachePath);

  // Now purge the old meta dir (GetActiveCacheRoot now returns new path)
  if (cacheChanging && !oldMetaDir.empty()) {
    bool exists = false;
    FileSystem::DirectoryExists(oldMetaDir.c_str(), exists);
    if (exists) {
      FileSystem::DirectoryDelete(oldMetaDir.c_str(), true);
      FileSystem::DirectoryCreate(oldMetaDir.c_str());
      Debug::Print("SettingsScene: purged old cache meta %s\n", oldMetaDir.c_str());
    }
  }

  AppSettings::Save();
  Context::GetSceneManager()->PopScene();
}

// ==========================================================================
// OpenPathBrowser
// ==========================================================================
void SettingsScene::OpenPathBrowser() {
  Context::GetSceneManager()->PushScene(
      new InstallPathScene(OnDownloadPathConfirmed, this, "Download Path"));
}

void SettingsScene::OpenCachePathBrowser() {
  Context::GetSceneManager()->PushScene(
      new InstallPathScene(OnCachePathConfirmed, this, "Cache Path"));
}

// ==========================================================================
// ClearImageCache -- deletes and recreates covers + screenshots dirs,
// then invalidates in-memory GPU textures so covers revert to placeholder.
// ==========================================================================
static void ClearImageCache() {
  std::string root = StoreList::GetActiveCacheRoot();
  std::string coversDir      = root + "\\Covers";
  std::string screenshotsDir = root + "\\Screenshots";
  FileSystem::DirectoryDelete(coversDir.c_str(), true);
  FileSystem::DirectoryDelete(screenshotsDir.c_str(), true);
  FileSystem::DirectoryCreate(coversDir.c_str());
  FileSystem::DirectoryCreate(screenshotsDir.c_str());
  ImageDownloader::ResetCachedCoverCount(); // resets to 0 (dirs now empty)
  StoreManager::InvalidateCovers();
}

// ==========================================================================
// Update
// ==========================================================================
void SettingsScene::Update() {
  // ---- Cache location picker ----
  if (mCachePickerOpen) {
    if (InputManager::ControllerPressed(ControllerDpadUp, -1)) {
      mCachePickerSel = (mCachePickerSel > 0) ? mCachePickerSel - 1 : 2;
      return;
    }
    if (InputManager::ControllerPressed(ControllerDpadDown, -1)) {
      mCachePickerSel = (mCachePickerSel < 2) ? mCachePickerSel + 1 : 0;
      return;
    }
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      mCacheLocation = (CacheLocation)mCachePickerSel;
      mCachePickerOpen = false;
      // If custom selected, immediately open path browser
      if (mCacheLocation == CacheLocationCustom) {
        OpenCachePathBrowser();
      }
      return;
    }
    if (InputManager::ControllerPressed(ControllerB, -1)) {
      mCachePickerOpen = false;
      return;
    }
    return;
  }

  // ---- After-install picker popup ----
  if (mPickerOpen) {
    if (InputManager::ControllerPressed(ControllerDpadUp, -1)) {
      mPickerSel = (mPickerSel > 0) ? mPickerSel - 1 : 2;
      return;
    }
    if (InputManager::ControllerPressed(ControllerDpadDown, -1)) {
      mPickerSel = (mPickerSel < 2) ? mPickerSel + 1 : 0;
      return;
    }
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      mAfterInstallAction = (AfterInstallAction)mPickerSel;
      mPickerOpen = false;
      return;
    }
    if (InputManager::ControllerPressed(ControllerB, -1)) {
      mPickerOpen = false;
      return;
    }
    return;
  }

  // ---- Clear failed confirm dialog ----
  if (mClearFailedConfirmOpen) {
    if (InputManager::ControllerPressed(ControllerY, -1)) {
      ImageDownloader::ClearFailedCovers();
      mClearFailedDone = true;
      mClearFailedConfirmOpen = false;
      return;
    }
    if (InputManager::ControllerPressed(ControllerB, -1) ||
        InputManager::ControllerPressed(ControllerA, -1)) {
      mClearFailedConfirmOpen = false;
      return;
    }
    return;
  }

  // ---- Clear cache confirm dialog ----
  if (mClearCacheConfirmOpen) {
    if (InputManager::ControllerPressed(ControllerY, -1)) {
      ClearImageCache();
      mClearCacheDone = true;
      mClearCacheConfirmOpen = false;
      return;
    }
    if (InputManager::ControllerPressed(ControllerB, -1) ||
        InputManager::ControllerPressed(ControllerA, -1)) {
      mClearCacheConfirmOpen = false;
      return;
    }
    return;
  }

  if (InputManager::ControllerPressed(ControllerB, -1) ||
      InputManager::ControllerPressed(ControllerStart, -1)) {
    SaveAndPop();
    return;
  }

  if (InputManager::ControllerPressed(ControllerDpadUp, -1)) {
    int32_t prev = mSelectedRow;
    mSelectedRow = (mSelectedRow > 0) ? mSelectedRow - 1 : ROW_COUNT - 1;
    if (mSelectedRow < prev) {
      mScrollOffset -= (ROW_H + 2.0f);
    } else {
      mScrollOffset = 999.0f; // wrapped to bottom -- clamp will fix in Render
    }
    if (mScrollOffset < 0.0f) {
      mScrollOffset = 0.0f;
    }
    return;
  }
  if (InputManager::ControllerPressed(ControllerDpadDown, -1)) {
    int32_t prev = mSelectedRow;
    mSelectedRow = (mSelectedRow < ROW_COUNT - 1) ? mSelectedRow + 1 : 0;
    if (mSelectedRow > prev) {
      mScrollOffset += (ROW_H + 2.0f);
    } else {
      mScrollOffset = 0.0f; // wrapped to top
    }
    return;
  }

  if (InputManager::ControllerPressed(ControllerA, -1)) {
    switch (mSelectedRow) {
    case 0:
      Context::GetSceneManager()->PushScene(new StoreManagerScene());
      break;
    case 1:
      OpenPathBrowser();
      break;
    case 2:
      mCachePickerSel = (int)mCacheLocation;
      mCachePickerOpen = true;
      break;
    case 3:
      mPickerSel = (int)mAfterInstallAction;
      mPickerOpen = true;
      break;
    case 4:
      mShowCachePartitions = !mShowCachePartitions;
      break;
    case 5:
      mPreCacheOnIdle = !mPreCacheOnIdle;
      break;
    case 6:
      mRetryFailedOnView = !mRetryFailedOnView;
      break;
    case 7:
      mClearFailedDone = false;
      mClearFailedConfirmOpen = true;
      break;
    case 8:
      mClearCacheDone = false;
      mClearCacheConfirmOpen = true;
      break;
    }
    return;
  }
}

// ==========================================================================
// Render helpers
// ==========================================================================
static const char *AfterInstallLabel(AfterInstallAction a) {
  switch (a) {
  case AfterInstallDelete:
    return "Always Delete";
  case AfterInstallKeep:
    return "Always Keep";
  case AfterInstallAsk:
    return "Ask Each Time";
  default:
    return "Unknown";
  }
}

static const char *CacheLocationLabel(CacheLocation loc) {
  switch (loc) {
  case CacheLocationTemp:   return "Default (TDATA)";
  case CacheLocationApp:    return "Application Path";
  case CacheLocationCustom: return "Custom Path";
  default:                  return "Unknown";
  }
}

static void DrawSection(float x, float y, float w, const char *label) {
  Drawing::DrawFilledRect(0x22ffffff, x, y, w, 1.0f);
  y += 5.0f;
  Font::DrawText(FONT_NORMAL, label, COLOR_FOCUS_HIGHLIGHT, x, y);
}

static void DrawRow(float x, float y, float w, bool selected, const char *label,
    const std::string &value, const char *rightHint = NULL) {
  uint32_t bg = selected ? 0xFF3C3C3C : 0xFF282828;
  Drawing::DrawFilledRect(bg, x, y, w, ROW_H - 2.0f);

  if (selected) {
    Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 3.0f, ROW_H - 2.0f);
  }

  float tx = x + 14.0f + (selected ? 3.0f : 0.0f);

  Font::DrawText(FONT_NORMAL, label, COLOR_TEXT_GRAY, tx, y + 8.0f);

  float maxValW = w - 28.0f - (rightHint ? 120.0f : 0.0f);
  std::string displayVal = Font::TruncateText(FONT_NORMAL, value, maxValW);
  Font::DrawText(FONT_NORMAL, displayVal.c_str(), COLOR_WHITE, tx, y + 34.0f);

  if (rightHint != NULL) {
    float hw = 0.0f;
    Font::MeasureText(FONT_NORMAL, rightHint, &hw);
    uint32_t hintCol = selected ? COLOR_FOCUS_HIGHLIGHT : 0xFF666666;
    Font::DrawText(FONT_NORMAL, rightHint, hintCol, x + w - hw - 14.0f,
        y + (ROW_H - 18.0f) * 0.5f);
  }
}

static void DrawCycleRow(float x, float y, float w, bool selected,
    const char *label, const char *value) {
  uint32_t bg = selected ? 0xFF3C3C3C : 0xFF282828;
  Drawing::DrawFilledRect(bg, x, y, w, ROW_H - 2.0f);

  if (selected) {
    Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 3.0f, ROW_H - 2.0f);
  }

  float tx = x + 14.0f + (selected ? 3.0f : 0.0f);
  Font::DrawText(FONT_NORMAL, label, COLOR_TEXT_GRAY, tx, y + 8.0f);

  if (selected) {
    float valW = 0.0f;
    Font::MeasureText(FONT_NORMAL, value, &valW);
    float midX = x + w * 0.5f;
    Font::DrawText(FONT_NORMAL, "<", COLOR_FOCUS_HIGHLIGHT,
        midX - valW * 0.5f - 22.0f, y + 36.0f);
    Font::DrawText(FONT_NORMAL, value, COLOR_WHITE, midX - valW * 0.5f,
        y + 36.0f);
    Font::DrawText(FONT_NORMAL, ">", COLOR_FOCUS_HIGHLIGHT,
        midX + valW * 0.5f + 8.0f, y + 36.0f);
  } else {
    Font::DrawText(FONT_NORMAL, value, COLOR_WHITE, tx, y + 36.0f);
  }
}

// ==========================================================================
// Render
// ==========================================================================
void SettingsScene::Render() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();
  float rowX = ROW_MARGIN;
  float rowW = screenW - ROW_MARGIN * 2.0f;
  float footerY = screenH - ASSET_FOOTER_HEIGHT;

  Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0,
      screenW, screenH);

  Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0,
      screenW, ASSET_HEADER_HEIGHT);
  Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0xFFFFFFFF, 16, 12,
      ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
  Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);

  float titleW = 0.0f;
  Font::MeasureText(FONT_LARGE, "Xbox Homebrew Store", &titleW);
  Font::DrawText(FONT_NORMAL, APP_VERSION, COLOR_TEXT_GRAY,
      60.0f + titleW + 10.0f, 20.0f);

  const char *sub = "Settings";
  float sw = 0.0f;
  Font::MeasureText(FONT_NORMAL, sub, &sw);
  Font::DrawText(FONT_NORMAL, sub, COLOR_TEXT_GRAY, screenW - sw - 16.0f,
      20.0f);

  float contentH = (SEC_HEAD_H + ROW_H + 2.0f) +
                   (SEC_GAP + SEC_HEAD_H + 2.0f * (ROW_H + 2.0f)) +
                   (SEC_GAP + SEC_HEAD_H + ROW_H + 2.0f) +
                   (SEC_GAP + SEC_HEAD_H + 5.0f * (ROW_H + 2.0f));
  float visibleH = footerY - BODY_TOP;
  float maxScroll = contentH - visibleH;
  if (maxScroll < 0.0f) {
    maxScroll = 0.0f;
  }
  if (mScrollOffset > maxScroll) {
    mScrollOffset = maxScroll;
  }
  if (mScrollOffset < 0.0f) {
    mScrollOffset = 0.0f;
  }

  float y = BODY_TOP - mScrollOffset;

  Drawing::BeginStencil(0.0f, BODY_TOP, screenW, visibleH);

  DrawSection(rowX, y, rowW, "Store");
  y += SEC_HEAD_H;
  std::string activeName = "";
  StoreEntry *active = StoreList::GetEntry(StoreList::GetActiveIndex());
  if (active != NULL) {
    activeName = std::string(active->name);
  }
  std::string storeCount =
      String::Format("%d configured", StoreList::GetCount());
  std::string storeDisplay = activeName + "  (" + storeCount + ")";
  DrawRow(rowX, y, rowW, mSelectedRow == 0, "Active Store", storeDisplay,
      mSelectedRow == 0 ? "[ Manage ]" : NULL);
  y += ROW_H + 2.0f;

  y += SEC_GAP;
  DrawSection(rowX, y, rowW, "Storage");
  y += SEC_HEAD_H;
  DrawRow(rowX, y, rowW, mSelectedRow == 1, "Download Path", mDownloadPath,
      mSelectedRow == 1 ? "[ Browse ]" : NULL);
  y += ROW_H + 2.0f;

  // Cache location row -- show custom path if set, otherwise show location label
  std::string cacheDisplay = CacheLocationLabel(mCacheLocation);
  if (mCacheLocation == CacheLocationCustom && !mCachePath.empty()) {
    cacheDisplay = mCachePath;
  }
  DrawRow(rowX, y, rowW, mSelectedRow == 2, "Cover Cache Location", cacheDisplay,
      mSelectedRow == 2 ? "[ Change ]" : NULL);
  y += ROW_H + 2.0f;

  y += SEC_GAP;
  DrawSection(rowX, y, rowW, "Install Behaviour");
  y += SEC_HEAD_H;
  DrawRow(rowX, y, rowW, mSelectedRow == 3, "After Install Action",
      AfterInstallLabel(mAfterInstallAction),
      mSelectedRow == 3 ? "[ Change ]" : NULL);
  y += ROW_H + 2.0f;

  y += SEC_GAP;
  DrawSection(rowX, y, rowW, "Advanced");
  y += SEC_HEAD_H;
  DrawRow(rowX, y, rowW, mSelectedRow == 4, "Show Cache Partitions",
      mShowCachePartitions ? "Enabled" : "Disabled",
      mSelectedRow == 4 ? "[ Toggle ]" : NULL);
  y += ROW_H + 2.0f;
  DrawRow(rowX, y, rowW, mSelectedRow == 5, "Pre-Cache Covers on Idle",
      mPreCacheOnIdle ? "Enabled" : "Disabled",
      mSelectedRow == 5 ? "[ Toggle ]" : NULL);
  y += ROW_H + 2.0f;
  DrawRow(rowX, y, rowW, mSelectedRow == 6,
      "Auto-Retry Failed Cover/Screenshot on App View",
      mRetryFailedOnView ? "Enabled" : "Disabled",
      mSelectedRow == 6 ? "[ Toggle ]" : NULL);
  y += ROW_H + 2.0f;
  {
    int32_t failCount = ImageDownloader::GetFailedCoverCount();
    std::string failDesc = mClearFailedDone
        ? "Done - idle downloader will retry on next scroll"
        : (failCount > 0
            ? String::Format("%d failed cover(s) and screenshot(s)", failCount)
            : "No failed downloads");
    DrawRow(rowX, y, rowW, mSelectedRow == 7, "Clear Failed Cover/Screenshot Markers",
        failDesc,
        (mSelectedRow == 7 && failCount > 0) ? "[ Clear ]" : NULL);
  }
  y += ROW_H + 2.0f;
  DrawRow(rowX, y, rowW, mSelectedRow == 8, "Clear Image Cache",
      mClearCacheDone ? "Done - all covers and screenshots removed"
                      : "Deletes all covers, screenshots and failed markers",
      mSelectedRow == 8 ? "[ Clear ]" : NULL);
  y += ROW_H + 2.0f;

  Drawing::EndStencil();

  bool dirty =
      (mDownloadPath != AppSettings::GetDownloadPath()) ||
      ((int)mAfterInstallAction != (int)AppSettings::GetAfterInstallAction()) ||
      (mShowCachePartitions != AppSettings::GetShowCachePartitions()) ||
      (mPreCacheOnIdle != AppSettings::GetPreCacheOnIdle()) ||
      (mRetryFailedOnView != AppSettings::GetRetryFailedOnView()) ||
      ((int)mCacheLocation != (int)AppSettings::GetCacheLocation()) ||
      (mCachePath != AppSettings::GetCachePath());
  if (dirty) {
    const char *hint = "* Unsaved changes";
    float hw = 0.0f;
    Font::MeasureText(FONT_NORMAL, hint, &hw);
    Font::DrawText(FONT_NORMAL, hint, COLOR_FOCUS_HIGHLIGHT,
        screenW - hw - 14.0f, footerY - 22.0f);
  }

  Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0.0f,
      footerY, screenW, ASSET_FOOTER_HEIGHT);

  float fx = 16.0f;
  float tw = 0.0f;
  D3DTexture *ftex = NULL;

#define DRAW_CTRL(iconName, lblStr)                                      \
  ftex = TextureHelper::GetControllerIcon(iconName);                     \
  if (ftex) {                                                            \
    Drawing::DrawTexturedRect(ftex, 0xffffffff, fx, footerY + 10.0f,     \
        ASSET_CONTROLLER_ICON_WIDTH,                                     \
        ASSET_CONTROLLER_ICON_HEIGHT);                                   \
    fx += ASSET_CONTROLLER_ICON_WIDTH + 4.0f;                            \
  }                                                                      \
  Font::DrawText(FONT_NORMAL, lblStr, COLOR_WHITE, fx, footerY + 12.0f); \
  Font::MeasureText(FONT_NORMAL, lblStr, &tw);                           \
  fx += tw + 20.0f;

  if (mSelectedRow == 2) {
    DRAW_CTRL("ButtonA", "Change");
  } else if (mSelectedRow == 3 || mSelectedRow == 4) {
    DRAW_CTRL("ButtonA", "Toggle");
  } else if (mSelectedRow == 5) {
    DRAW_CTRL("ButtonA", "Clear Cache");
  } else {
    DRAW_CTRL("ButtonA", mSelectedRow == 0 ? "Manage Stores" : "Browse");
  }
  DRAW_CTRL("Dpad", "Navigate");
  DRAW_CTRL("ButtonB", "Back");

#undef DRAW_CTRL

  if (mCachePickerOpen) {
    RenderCacheLocationPicker();
  }
  if (mPickerOpen) {
    RenderPicker();
  }
  if (mClearFailedConfirmOpen) {
    RenderClearFailedConfirm();
  }
  if (mClearCacheConfirmOpen) {
    RenderClearCacheConfirm();
  }
}

// ==========================================================================
// RenderPicker - small centred popup with 3 options
// ==========================================================================

// ==========================================================================
// RenderCacheLocationPicker
// ==========================================================================
void SettingsScene::RenderCacheLocationPicker() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xBB000000, 0.0f, 0.0f, screenW, screenH);

  const char *opts[3] = {"Default (TDATA)", "Application Path", "Custom Path"};
  const float ITEM_H = 40.0f;
  const float PAD = 16.0f;
  float panelW = 300.0f;
  float panelH = PAD + (float)3 * ITEM_H + PAD + 32.0f;
  float px = (screenW - panelW) * 0.5f;
  float py = (screenH - panelH) * 0.5f;

  Drawing::DrawFilledRect(0xFF1E1E1E, px, py, panelW, panelH);
  Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 3.0f);

  int i;
  for (i = 0; i < 3; i++) {
    float iy = py + PAD + (float)i * ITEM_H;
    bool sel = (mCachePickerSel == i);
    bool cur = ((int)mCacheLocation == i);

    if (sel) {
      Drawing::DrawFilledRect(0xFF2E2E2E, px, iy, panelW, ITEM_H - 2.0f);
      Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, iy, 3.0f, ITEM_H - 2.0f);
    }

    uint32_t col = sel ? COLOR_WHITE : COLOR_TEXT_GRAY;
    Font::DrawText(FONT_NORMAL, opts[i], col, (px + 16.0f), (iy + 11.0f));

    if (cur) {
      Font::DrawText(FONT_LARGE, "*", COLOR_FOCUS_HIGHLIGHT,
          (px + panelW - 26.0f), (iy + 6.0f));
    }
  }

  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float selW = 0.0f;
  float canW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Select", &selW);
  Font::MeasureText(FONT_NORMAL, "Cancel", &canW);
  float hintW = (iW + 4.0f + selW) + 16.0f + (iW + 4.0f + canW);
  float hx = px + (panelW - hintW) * 0.5f;
  float hy = py + panelH - iH - 8.0f;

  D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
  if (iconA) {
    Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Select", COLOR_TEXT_GRAY, hx, (hy + 2.0f));
  hx += selW + 16.0f;
  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB) {
    Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Cancel", COLOR_TEXT_GRAY, hx, (hy + 2.0f));
}

// ==========================================================================
// RenderPicker
// ==========================================================================
void SettingsScene::RenderPicker() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xBB000000, 0.0f, 0.0f, screenW, screenH);

  const char *opts[3] = {"Always Delete", "Always Keep", "Ask Each Time"};
  const float ITEM_H = 40.0f;
  const float PAD = 16.0f;
  float panelW = 280.0f;
  float panelH = PAD + (float)3 * ITEM_H + PAD + 32.0f;
  float px = (screenW - panelW) * 0.5f;
  float py = (screenH - panelH) * 0.5f;

  Drawing::DrawFilledRect(0xFF1E1E1E, px, py, panelW, panelH);
  Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 3.0f);

  int i;
  for (i = 0; i < 3; i++) {
    float iy = py + PAD + (float)i * ITEM_H;
    bool sel = (mPickerSel == i);
    bool cur = (mAfterInstallAction == i);

    if (sel) {
      Drawing::DrawFilledRect(0xFF2E2E2E, px, iy, panelW, ITEM_H - 2.0f);
    }
    if (sel) {
      Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, iy, 3.0f,
          ITEM_H - 2.0f);
    }

    uint32_t col = sel ? COLOR_WHITE : COLOR_TEXT_GRAY;
    Font::DrawText(FONT_NORMAL, opts[i], col, (px + 16.0f), (iy + 11.0f));

    if (cur) {
      Font::DrawText(FONT_LARGE, "*", COLOR_FOCUS_HIGHLIGHT,
          (px + panelW - 26.0f), (iy + 6.0f));
    }
  }

  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float selW = 0.0f;
  float canW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Select", &selW);
  Font::MeasureText(FONT_NORMAL, "Cancel", &canW);
  float hintW = (iW + 4.0f + selW) + 16.0f + (iW + 4.0f + canW);
  float hx = px + (panelW - hintW) * 0.5f;
  float hy = py + panelH - iH - 8.0f;

  D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
  if (iconA) {
    Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Select", COLOR_TEXT_GRAY, hx, (hy + 2.0f));
  hx += selW + 16.0f;
  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB) {
    Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Cancel", COLOR_TEXT_GRAY, hx, (hy + 2.0f));
}

// ==========================================================================
// RenderClearCacheConfirm
// ==========================================================================
void SettingsScene::RenderClearCacheConfirm() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, screenW, screenH);

  const float panelW = 440.0f;
  const float panelH = 130.0f;
  float px = (screenW - panelW) * 0.5f;
  float py = (screenH - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(0xFFEF5350, px, py, panelW, 4.0f);

  Font::DrawText(FONT_NORMAL, "Clear Image Cache?", COLOR_WHITE, px + 20.0f,
      py + 18.0f);
  Font::DrawTextWrapped(FONT_NORMAL,
      "This will delete all cached covers and "
      "screenshots.\nThey will be re-downloaded on next use.",
      COLOR_TEXT_GRAY, px + 20.0f, py + 44.0f,
      panelW - 40.0f);

  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float confW = 0.0f;
  float canW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Confirm", &confW);
  Font::MeasureText(FONT_NORMAL, "Cancel", &canW);
  float hintW = (iW + 4.0f + confW) + 16.0f + (iW + 4.0f + canW);
  float hx = px + (panelW - hintW) * 0.5f;
  float hy = py + panelH - iH - 10.0f;

  D3DTexture *iconY = TextureHelper::GetControllerIcon("ButtonY");
  if (iconY) {
    Drawing::DrawTexturedRect(iconY, 0xFFFFFFFF, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Confirm", COLOR_TEXT_GRAY, hx, hy + 2.0f);
  hx += confW + 16.0f;
  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB) {
    Drawing::DrawTexturedRect(iconB, 0xFFFFFFFF, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Cancel", COLOR_TEXT_GRAY, hx, hy + 2.0f);
}

// ==========================================================================
// RenderClearFailedConfirm
// ==========================================================================
void SettingsScene::RenderClearFailedConfirm() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, screenW, screenH);

  const float panelW = 460.0f;
  const float panelH = 130.0f;
  float px = (screenW - panelW) * 0.5f;
  float py = (screenH - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 4.0f);

  int32_t failCount = ImageDownloader::GetFailedCoverCount();
  std::string title = String::Format("Clear %d failed download(s)?", failCount);
  Font::DrawText(FONT_NORMAL, title.c_str(), COLOR_WHITE, px + 20.0f, py + 18.0f);
  Font::DrawTextWrapped(FONT_NORMAL,
      "Removes all .fail markers so covers and screenshots\n"
      "can be re-attempted by the idle downloader.",
      COLOR_TEXT_GRAY, px + 20.0f, py + 44.0f, panelW - 40.0f);

  float iW   = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH   = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float confW = 0.0f, canW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Confirm", &confW);
  Font::MeasureText(FONT_NORMAL, "Cancel",  &canW);
  float hintW = (iW + 4.0f + confW) + 16.0f + (iW + 4.0f + canW);
  float hx    = px + (panelW - hintW) * 0.5f;
  float hy    = py + panelH - iH - 10.0f;

  D3DTexture *iconY = TextureHelper::GetControllerIcon("ButtonY");
  if (iconY) {
    Drawing::DrawTexturedRect(iconY, 0xFFFFFFFF, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Confirm", COLOR_TEXT_GRAY, hx, hy + 2.0f);
  hx += confW + 16.0f;
  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB) {
    Drawing::DrawTexturedRect(iconB, 0xFFFFFFFF, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Cancel", COLOR_TEXT_GRAY, hx, hy + 2.0f);
}