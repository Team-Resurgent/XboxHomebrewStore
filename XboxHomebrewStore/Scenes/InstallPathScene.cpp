#include "InstallPathScene.h"
#include "SceneManager.h"

#include "..\AppSettings.h"
#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\DriveMount.h"
#include "..\FileSystem.h"
#include "..\Font.h"
#include "..\InputManager.h"
#include "..\Math.h"
#include "..\OnScreenKeyboard.h"
#include "..\String.h"
#include "..\TextureHelper.h"

// ==========================================================================
// Auto-repeat timing
// ==========================================================================
#define IP_REPEAT_INITIAL_MS 400
#define IP_REPEAT_INTERVAL_MS 70
#define IP_TRIG_INIT_MIN_MS 140
#define IP_TRIG_INIT_MAX_MS 300
#define IP_TRIG_INTV_FAST_MS 30
#define IP_TRIG_INTV_SLOW_MS 180
#define IP_TRIG_DEADZONE 4

#define FIRE_ON_HOLD(btnPress, btnDown, nextTs)                               \
  ((btnPress) ? ((nextTs = GetTickCount() + IP_REPEAT_INITIAL_MS), 1)          \
              : ((btnDown) && ((int)(GetTickCount() - (nextTs)) >= 0)           \
                        ? ((nextTs = GetTickCount() + IP_REPEAT_INTERVAL_MS), 1) \
                        : 0))

static DWORD IP_MapInvQuad(DWORD lo, DWORD hi, BYTE p) {
  DWORD num = (DWORD)p * (DWORD)p;
  DWORD span = (hi > lo) ? (hi - lo) : 0;
  DWORD dec = (span * num) / (255u * 255u);
  DWORD val = (hi > dec) ? (hi - dec) : lo;
  if (val < lo) {
    val = lo;
  }
  if (val > hi) {
    val = hi;
  }
  return val;
}
static DWORD IP_TrigInitDelay(BYTE p) {
  return (p <= IP_TRIG_DEADZONE)
             ? IP_TRIG_INIT_MAX_MS
             : IP_MapInvQuad(IP_TRIG_INIT_MIN_MS, IP_TRIG_INIT_MAX_MS, p);
}
static DWORD IP_TrigInterval(BYTE p) {
  return (p <= IP_TRIG_DEADZONE)
             ? IP_TRIG_INTV_SLOW_MS
             : IP_MapInvQuad(IP_TRIG_INTV_FAST_MS, IP_TRIG_INTV_SLOW_MS, p);
}
static int IP_FireTrigger(bool pressed, bool down, BYTE p, DWORD *ts,
    DWORD now) {
  if (pressed) {
    *ts = now + IP_TrigInitDelay(p);
    return 1;
  }
  if (down && (now - *ts) >= 0) {
    *ts = now + IP_TrigInterval(p);
    return 1;
  }
  return 0;
}

// ==========================================================================
// Constructor / Destructor
// ==========================================================================
InstallPathScene::InstallPathScene(InstallPathConfirmedCb callback,
    void *userData, const char *title)
    : mCallback(callback), mUserData(userData),
      mTitle(title != nullptr ? title : "Select Install Path"),
      mInitialized(false), mSelectedIndex(0), mRepeatNext_Down(0),
      mRepeatNext_Up(0), mRepeatNext_Left(0), mRepeatNext_Right(0),
      mRepeatNext_LT(0), mRepeatNext_RT(0), mKbPurpose(KbNone),
      mDeleteOpen(false),
  mDeleting(false),
  mDeleteCancelRequested(false),
  mDeleteCurrent(0),
  mDeleteTotal(0),
  mDeleteThread(NULL) {
  memset(mDeleteCurrentName, 0, sizeof(mDeleteCurrentName));
  mCurrentPath = "";
  RefreshList();
  mInitialized = true;
}

InstallPathScene::~InstallPathScene() {
  if (mDeleteThread != NULL) {
    mDeleteCancelRequested = true;
    WaitForSingleObject(mDeleteThread, INFINITE);
    CloseHandle(mDeleteThread);
    mDeleteThread = NULL;
  }
}

// ==========================================================================
// RefreshList
// ==========================================================================
void InstallPathScene::RefreshList(const std::string &selectName) {
  mItems.clear();

  if (mCurrentPath.empty()) {
    bool showCache = AppSettings::GetShowCachePartitions();
    std::vector<std::string> drives = DriveMount::GetMountedDrives();
    for (size_t i = 0; i < drives.size(); i++) {
      const std::string &d = drives[i];
      // Always skip DVD-ROM - can't install to optical disc
      if (String::EqualsIgnoreCase(d, "DVD-ROM")) {
        continue;
      }
      // Skip X/Y/Z cache partitions unless user has enabled them in settings
      if (!showCache && d.size() >= 2) {
        char last = (char)toupper(d[d.size() - 1]);
        char prev = d[d.size() - 2];
        if (prev == '-' && (last == 'X' || last == 'Y' || last == 'Z')) {
          continue;
        }
      }

      FileInfoDetail fid;
      fid.path = d + ":\\";
      fid.isDirectory = true;
      fid.isFile = false;
      mItems.push_back(fid);
    }
    mSelectedIndex = 0;
  } else {
    std::vector<FileInfoDetail> all =
        FileSystem::FileGetFileInfoDetails(mCurrentPath);
    for (size_t i = 0; i < all.size(); i++) {
      if (all[i].isDirectory) {
        mItems.push_back(all[i]);
      }
    }

    mSelectedIndex = 0;
    if (!selectName.empty()) {
      for (int32_t i = 0; i < (int32_t)mItems.size(); i++) {
        if (String::EqualsIgnoreCase(FileSystem::GetFileName(mItems[i].path),
                selectName)) {
          mSelectedIndex = i;
          break;
        }
      }
    }
  }

  int32_t count = (int32_t)mItems.size();
  if (mSelectedIndex >= count) {
    mSelectedIndex = (count > 0) ? count - 1 : 0;
  }
}

// ==========================================================================
// Display helpers
// ==========================================================================
std::string InstallPathScene::GetDisplayName(int32_t index) const {
  if (index < 0 || index >= (int32_t)mItems.size()) {
    return "";
  }
  const std::string &p = mItems[index].path;
  if (mCurrentPath.empty()) {
    std::string n = String::RightTrim(p, '\\');
    if (!n.empty() && n[n.size() - 1] == ':') {
      n = n.substr(0, n.size() - 1);
    }
    return n;
  }
  return FileSystem::GetFileName(p);
}

char InstallPathScene::GetKeyChar(const std::string &name) const {
  for (size_t i = 0; i < name.size(); i++) {
    unsigned char c = (unsigned char)name[i];
    if (isalnum(c)) {
      return (char)toupper(c);
    }
  }
  return '#';
}

int32_t InstallPathScene::JumpToNextLetter(int32_t start, int dir) const {
  int32_t n = (int32_t)mItems.size();
  if (n <= 1) {
    return start;
  }
  dir = (dir < 0) ? -1 : 1;
  char curKey = GetKeyChar(GetDisplayName(start));
  int32_t idx = start;
  for (int32_t s = 0; s < n; s++) {
    idx += dir;
    if (idx >= n) {
      idx = 0;
    }
    if (idx < 0) {
      idx = n - 1;
    }
    if (GetKeyChar(GetDisplayName(idx)) != curKey) {
      return idx;
    }
  }
  return start;
}

// ==========================================================================
// Navigation
// ==========================================================================
void InstallPathScene::NavigateInto() {
  if (mItems.empty()) {
    return;
  }
  const FileInfoDetail &fid = mItems[mSelectedIndex];
  if (!fid.isDirectory) {
    return;
  }

  mCurrentPath = fid.path;
  if (!mCurrentPath.empty() && mCurrentPath[mCurrentPath.size() - 1] != '\\') {
    mCurrentPath += '\\';
  }

  RefreshList();
}

void InstallPathScene::NavigateUp() {
  if (mCurrentPath.empty()) {
    Context::GetSceneManager()->PopScene();
    return;
  }

  std::string leaving = String::RightTrim(mCurrentPath, '\\');
  std::string leafName = FileSystem::GetFileName(leaving);
  std::string parent = FileSystem::GetDirectory(leaving);

  mCurrentPath = parent.empty() ? "" : parent + "\\";
  RefreshList(leafName);
}

void InstallPathScene::ConfirmCurrent() {
  if (mCurrentPath.empty()) {
    return;
  }
  mChosenPath = mCurrentPath;
  if (mCallback != nullptr) {
    mCallback(mChosenPath, mUserData);
  }
  Context::GetSceneManager()->PopScene();
}

// ==========================================================================
// Keyboard result handler
// ==========================================================================
void InstallPathScene::HandleKeyboardResult() {
  std::string name = mKeyboard.GetResult();
  if (name.empty()) {
    mKbPurpose = KbNone;
    return;
  }

  if (mKbPurpose == KbCreate) {
    std::string newPath = mCurrentPath + name;
    FileSystem::DirectoryCreate(newPath);
    RefreshList(name);
  } else if (mKbPurpose == KbRename && !mItems.empty()) {
    std::string oldPath = String::RightTrim(mItems[mSelectedIndex].path, '\\');
    std::string parentDir = FileSystem::GetDirectory(oldPath);
    std::string newPath = parentDir + "\\" + name;
    FileSystem::FileMove(oldPath, newPath);
    RefreshList(name);
  }

  mKbPurpose = KbNone;
}

// ==========================================================================
// Delete
// ==========================================================================
void InstallPathScene::DeleteOpen() {
  if (mItems.empty() || mCurrentPath.empty()) {
    return;
  }
  mDeleteName = GetDisplayName(mSelectedIndex);
  mDeletePath = String::RightTrim(mItems[mSelectedIndex].path, '\\');
  mDeleteOpen = true;
}

void InstallPathScene::DeleteConfirm() {
  mDeleteOpen = false;
  mDeleting = true;
  mDeleteCancelRequested = false;
  mDeleteCurrent = 0;
  mDeleteTotal = 0; // updated live as files are deleted
  memset(mDeleteCurrentName, 0, sizeof(mDeleteCurrentName));
  mDeleteThread = CreateThread(NULL, 0, DeleteThreadProc, this, 0, NULL);
}

// ==========================================================================
// Update
// ==========================================================================
void InstallPathScene::Update() {
  // ---- Keyboard overlay takes priority ----
  if (mKeyboard.IsOpen()) {
    mKeyboard.Update();

    if (mKeyboard.IsConfirmed()) {
      HandleKeyboardResult();
    }
    return;
  }

  // ---- Delete confirmation ----
  if (mDeleting) {
    if (InputManager::ControllerPressed(ControllerB, -1)) {
      mDeleteCancelRequested = true;
    }
    return;
  }

  if (mDeleteOpen) {
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      DeleteConfirm();
      return;
    }
    if (InputManager::ControllerPressed(ControllerB, -1)) {
      mDeleteOpen = false;
      return;
    }
    return;
  }

  // ---- Browse mode ----
  if (InputManager::ControllerPressed(ControllerB, -1)) {
    Context::GetSceneManager()->PopScene();
    return;
  }
  if (InputManager::ControllerPressed(ControllerX, -1)) {
    NavigateUp();
    return;
  }
  if (InputManager::ControllerPressed(ControllerA, -1)) {
    NavigateInto();
    return;
  }
  if (InputManager::ControllerPressed(ControllerStart, -1)) {
    ConfirmCurrent();
    return;
  }
  if (InputManager::ControllerPressed(ControllerY, -1)) {
    if (!mCurrentPath.empty()) {
      mKbPurpose = KbCreate;
      mKeyboard.Open("New Folder Name", "", 64, KbFilterFilename,
          COLOR_FOCUS_HIGHLIGHT);
    }
    return;
  }
  if (InputManager::ControllerPressed(ControllerWhite, -1)) {
    if (!mCurrentPath.empty() && !mItems.empty()) {
      mKbPurpose = KbRename;
      mKeyboard.Open("Rename Folder", GetDisplayName(mSelectedIndex), 64,
          KbFilterFilename, 0xFF42A5F5);
    }
    return;
  }
  if (InputManager::ControllerPressed(ControllerBlack, -1)) {
    if (!mCurrentPath.empty() && !mItems.empty()) {
      DeleteOpen();
    }
    return;
  }

  // ---- D-Pad / trigger navigation ----
  int32_t count = (int32_t)mItems.size();
  if (count == 0) {
    return;
  }

  DWORD now = GetTickCount();
  ControllerState cs;
  bool hasState = InputManager::TryGetControllerState(-1, &cs);

  {
    bool pressed = InputManager::ControllerPressed(ControllerDpadDown, -1);
    bool down = hasState && cs.Buttons[ControllerDpadDown];
    if (FIRE_ON_HOLD(pressed, down, mRepeatNext_Down)) {
      mSelectedIndex = (mSelectedIndex < count - 1) ? mSelectedIndex + 1 : 0;
    }
  }
  {
    bool pressed = InputManager::ControllerPressed(ControllerDpadUp, -1);
    bool down = hasState && cs.Buttons[ControllerDpadUp];
    if (FIRE_ON_HOLD(pressed, down, mRepeatNext_Up)) {
      mSelectedIndex = (mSelectedIndex > 0) ? mSelectedIndex - 1 : count - 1;
    }
  }
  {
    bool pressed = InputManager::ControllerPressed(ControllerDpadLeft, -1);
    bool down = hasState && cs.Buttons[ControllerDpadLeft];
    if (FIRE_ON_HOLD(pressed, down, mRepeatNext_Left)) {
      mSelectedIndex = JumpToNextLetter(mSelectedIndex, -1);
    }
  }
  {
    bool pressed = InputManager::ControllerPressed(ControllerDpadRight, -1);
    bool down = hasState && cs.Buttons[ControllerDpadRight];
    if (FIRE_ON_HOLD(pressed, down, mRepeatNext_Right)) {
      mSelectedIndex = JumpToNextLetter(mSelectedIndex, +1);
    }
  }
  {
    bool pressed = InputManager::ControllerPressed(ControllerLTrigger, -1);
    bool down = hasState && cs.Buttons[ControllerLTrigger];
    BYTE p = down ? 180 : 0;
    if (IP_FireTrigger(pressed, down, p, &mRepeatNext_LT, now)) {
      mSelectedIndex = (mSelectedIndex > 0) ? mSelectedIndex - 1 : count - 1;
    }
  }
  {
    bool pressed = InputManager::ControllerPressed(ControllerRTrigger, -1);
    bool down = hasState && cs.Buttons[ControllerRTrigger];
    BYTE p = down ? 180 : 0;
    if (IP_FireTrigger(pressed, down, p, &mRepeatNext_RT, now)) {
      mSelectedIndex = (mSelectedIndex < count - 1) ? mSelectedIndex + 1 : 0;
    }
  }
}

// ==========================================================================
int32_t InstallPathScene::CountFiles(const std::string &path) {
  int32_t count = 0;
  std::vector<FileInfoDetail> items = FileSystem::FileGetFileInfoDetails(path);
  for (size_t i = 0; i < items.size(); i++) {
    if (items[i].isDirectory)
      count += CountFiles(items[i].path);
    else
      count++;
  }
  return count;
}

// ==========================================================================
bool InstallPathScene::DeleteRecursive(const std::string &path,
    volatile bool *pCancel,
    volatile int32_t *pCurrent, volatile int32_t *pTotal,
    char *pCurrentName, int32_t currentNameSize) {
  std::vector<FileInfoDetail> items = FileSystem::FileGetFileInfoDetails(path);
  for (size_t i = 0; i < items.size(); i++) {
    if (*pCancel) return false;
    const FileInfoDetail &item = items[i];
    std::string name = FileSystem::GetFileName(item.path);
    strncpy(pCurrentName, name.c_str(), currentNameSize - 1);
    pCurrentName[currentNameSize - 1] = '\0';
    if (item.isDirectory) {
      DeleteRecursive(item.path, pCancel, pCurrent, pTotal, pCurrentName, currentNameSize);
      RemoveDirectoryA(item.path.c_str());
    } else {
      FileSystem::FileDelete(item.path);
      InterlockedIncrement((LPLONG)pCurrent);
      InterlockedIncrement((LPLONG)pTotal);
    }
  }
  return !(*pCancel);
}

// ==========================================================================
DWORD WINAPI InstallPathScene::DeleteThreadProc(LPVOID param) {
  InstallPathScene *scene = (InstallPathScene *)param;
  if (scene == NULL) return 0;
  DeleteRecursive(scene->mDeletePath, &scene->mDeleteCancelRequested,
      &scene->mDeleteCurrent, &scene->mDeleteTotal,
      scene->mDeleteCurrentName, sizeof(scene->mDeleteCurrentName));
  if (!scene->mDeleteCancelRequested)
    RemoveDirectoryA(scene->mDeletePath.c_str());
  CloseHandle(scene->mDeleteThread);
  scene->mDeleteThread = NULL;
  scene->mDeleting = false;
  int32_t prev = scene->mSelectedIndex;
  scene->RefreshList();
  int32_t count = (int32_t)scene->mItems.size();
  scene->mSelectedIndex = (prev < count) ? prev : (count > 0 ? count - 1 : 0);
  return 0;
}

// ==========================================================================
// Render
// ==========================================================================
void InstallPathScene::Render() {
  Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0,
      (float)Context::GetScreenWidth(),
      (float)Context::GetScreenHeight());

  RenderHeader();
  RenderFooter();
  RenderList();

  if (mDeleteOpen) {
    RenderDeleteConfirm();
  } else if (mDeleting) {
    RenderDeleteProgress();
  }
  if (mKeyboard.IsOpen()) {
    mKeyboard.Render();
  }
}

// --------------------------------------------------------------------------
void InstallPathScene::RenderHeader() {
  Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0,
      (float)Context::GetScreenWidth(),
      ASSET_HEADER_HEIGHT);
  Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
  Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386, 16, 12,
      ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
}

// --------------------------------------------------------------------------
void InstallPathScene::DrawFooterControl(float &x, float footerY,
    const char *iconName,
    const char *label) {
  const float iconW = ASSET_CONTROLLER_ICON_WIDTH;
  const float iconH = ASSET_CONTROLLER_ICON_HEIGHT;
  float textWidth = 0.0f;

  D3DTexture *icon = TextureHelper::GetControllerIcon(iconName);
  if (icon != nullptr) {
    Drawing::DrawTexturedRect(icon, 0xffffffff, x, footerY + 10.0f, iconW,
        iconH);
    x += iconW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, label, COLOR_WHITE, x, (footerY + 12.0f));
  Font::MeasureText(FONT_NORMAL, label, &textWidth);
  x += textWidth + 20.0f;
}

// --------------------------------------------------------------------------
void InstallPathScene::RenderFooter() {
  float footerY = (float)Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
  float x = 16.0f;

  Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0.0f,
      footerY, (float)Context::GetScreenWidth(),
      ASSET_FOOTER_HEIGHT);

  if (mDeleteOpen) {
    DrawFooterControl(x, footerY, "ButtonA", "Delete");
    DrawFooterControl(x, footerY, "ButtonB", "Cancel");
  } else if (mCurrentPath.empty()) {
    DrawFooterControl(x, footerY, "ButtonA", "Enter Drive");
    DrawFooterControl(x, footerY, "ButtonB", "Cancel");
    DrawFooterControl(x, footerY, "Dpad", "Navigate");
  } else {
    DrawFooterControl(x, footerY, "ButtonA", "Open");
    DrawFooterControl(x, footerY, "Start", "Confirm Here");
    DrawFooterControl(x, footerY, "ButtonX", "Go Up");
    DrawFooterControl(x, footerY, "ButtonY", "New Folder");
    DrawFooterControl(x, footerY, "ButtonWhite", "Rename");
    DrawFooterControl(x, footerY, "ButtonBlack", "Delete");
    DrawFooterControl(x, footerY, "Dpad", "Navigate");
  }
}

// ==========================================================================
void InstallPathScene::RenderDeleteProgress() {
  const float w = (float)Context::GetScreenWidth();
  const float h = (float)Context::GetScreenHeight();
  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, w, h);
  const float panelW = 500.0f;
  const float panelH = 130.0f;
  float px = (w - panelW) * 0.5f;
  float py = (h - panelH) * 0.5f;
  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(0xFFEF5350, px, py, panelW, 4.0f);
  Font::DrawText(FONT_NORMAL, "Deleting...", COLOR_WHITE, px + 16.0f, py + 14.0f);
  const float barX = px + 16.0f;
  const float barY = py + 44.0f;
  const float barW = panelW - 32.0f;
  const float barH = 20.0f;
  Drawing::DrawFilledRect(COLOR_SECONDARY, barX, barY, barW, barH);
  int32_t total = mDeleteTotal;
  int32_t current = mDeleteCurrent;
  // Show progress - since we delete and count simultaneously
  // just pulse the bar based on file count
  if (current > 0) {
    float pulse = (float)(current % 20) / 20.0f;
    Drawing::DrawFilledRect(COLOR_DOWNLOAD, barX, barY, barW * pulse, barH);
  }
  std::string prog = String::Format("%d files deleted", current);
  Font::DrawText(FONT_NORMAL, prog, COLOR_TEXT_GRAY, barX, barY + barH + 6.0f);
  if (mDeleteCurrentName[0] != '\0') {
    Font::DrawText(FONT_NORMAL, mDeleteCurrentName, COLOR_TEXT_GRAY,
        barX, barY + barH + 22.0f);
  }
  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float lblW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Cancel", &lblW);
  float hx = px + (panelW - iW - 4.0f - lblW) * 0.5f;
  float hy = py + panelH - 28.0f;
  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB != NULL) {
    Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy - 2.0f, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Cancel", COLOR_WHITE, hx, hy);
}

// --------------------------------------------------------------------------
void InstallPathScene::RenderList() {
  const float screenW = (float)Context::GetScreenWidth();
  const float screenH = (float)Context::GetScreenHeight();
  const float listTop = ASSET_SIDEBAR_Y;
  const float listBot = screenH - ASSET_FOOTER_HEIGHT;
  const float rowH = 40.0f;

  Font::DrawText(FONT_NORMAL, mTitle.c_str(), COLOR_WHITE, 16, (listTop + 6));

  if (!mCurrentPath.empty()) {
    Font::DrawText(FONT_NORMAL, mCurrentPath.c_str(), COLOR_TEXT_GRAY, 16,
        (listTop + 22));
  }

  const float itemsTop = listTop + 50.0f;
  const float itemsH = listBot - itemsTop;
  int32_t maxVisible = (int32_t)(itemsH / rowH);
  int32_t count = (int32_t)mItems.size();

  if (count == 0) {
    float yc = itemsTop + itemsH * 0.5f - 20.0f;
    Font::DrawText(FONT_NORMAL, "No folders found.", COLOR_TEXT_GRAY, 24, yc);
    if (!mCurrentPath.empty()) {
      Font::DrawText(FONT_NORMAL, "Press Y to create a new folder here.",
          COLOR_TEXT_GRAY, 24, (yc + 30.0f));
    }
    return;
  }

  int32_t start = 0;
  if (count > maxVisible) {
    start = Math::ClampInt32(mSelectedIndex - maxVisible / 2, 0,
        count - maxVisible);
  }

  int32_t visibleCount = Math::MinInt32(start + maxVisible, count) - start;

  Drawing::BeginStencil(0.0f, itemsTop, screenW, itemsH);

  for (int32_t i = 0; i < visibleCount; i++) {
    int32_t idx = start + i;
    float ry = itemsTop + i * rowH;
    bool sel = (idx == mSelectedIndex);

    if (sel) {
      Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, 0.0f, ry, screenW,
          rowH - 2.0f);
    } else if (i % 2 == 0) {
      Drawing::DrawFilledRect(0x18ffffff, 0.0f, ry, screenW, rowH - 2.0f);
    }

    uint32_t colMain = sel ? COLOR_WHITE : COLOR_TEXT_GRAY;
    uint32_t colFolder = sel ? COLOR_FOLDER_SEL : COLOR_FOLDER;

    if (sel) {
      Font::DrawText(FONT_NORMAL, ">", COLOR_WHITE, 8, (ry + 10));
    }

    const float iconW = (float)ASSET_CONTROLLER_ICON_WIDTH;
    const float iconH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
    float iconY = ry + (rowH - iconH) * 0.5f;
    D3DTexture *itemIcon = mCurrentPath.empty()
                               ? TextureHelper::GetDriveIcon()
                               : TextureHelper::GetFolderIcon();

    if (itemIcon != NULL) {
      uint32_t colIcon = mCurrentPath.empty() ? colMain : colFolder;
      Drawing::DrawTexturedRect(itemIcon, colIcon, 24.0f, iconY, iconW, iconH);
    }

    Font::DrawText(FONT_NORMAL, GetDisplayName(idx).c_str(), colMain, 54.0f,
        (ry + 10));
  }

  Drawing::EndStencil();

  std::string counter =
      String::Format("Item %d of %d", mSelectedIndex + 1, count);
  float cw = 0.0f;
  Font::MeasureText(FONT_NORMAL, counter, &cw);
  Font::DrawText(FONT_NORMAL, counter.c_str(), COLOR_TEXT_GRAY,
      (screenW - 16.0f - cw), (listBot - 18.0f));
}

// --------------------------------------------------------------------------
void InstallPathScene::RenderDeleteConfirm() {
  const float screenW = (float)Context::GetScreenWidth();
  const float screenH = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, screenW, screenH);

  const float panelW = 500.0f;
  const float panelH = 130.0f;
  float px = (screenW - panelW) * 0.5f;
  float py = (screenH - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(0xFFEF5350, px, py, panelW, 4.0f);

  Font::DrawText(FONT_NORMAL, "Delete Folder?", COLOR_WHITE, (px + 16.0f),
      (py + 16.0f));

  std::string msg =
      String::Format("'%s'  -  This cannot be undone.", mDeleteName.c_str());
  Font::DrawText(FONT_NORMAL, msg.c_str(), COLOR_TEXT_GRAY, (px + 16.0f),
      (py + 46.0f));

  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float delW = 0.0f;
  float canW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Delete", &delW);
  Font::MeasureText(FONT_NORMAL, "Cancel", &canW);
  float pairW = (iW + 4.0f + delW) + 24.0f + (iW + 4.0f + canW);
  float hx = px + (panelW - pairW) * 0.5f;
  float hy = py + panelH - 36.0f;

  D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
  if (iconA) {
    Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Delete", 0xFFEF5350, hx, hy);
  hx += delW + 24.0f;

  D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
  if (iconB) {
    Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Cancel", COLOR_WHITE, hx, hy);
}