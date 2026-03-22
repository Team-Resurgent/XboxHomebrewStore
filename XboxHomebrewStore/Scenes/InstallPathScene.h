#pragma once

#include "..\DriveMount.h"
#include "..\FileSystem.h"
#include "..\Main.h"
#include "..\OnScreenKeyboard.h"
#include "Scene.h"

// Called when the user confirms a folder path.
typedef void (*InstallPathConfirmedCb)(const std::string &chosenPath,
    void *userData);

// ---------------------------------------------------------------------------
// InstallPathScene - drive / folder browser for choosing an install path
//
// Browse mode controls
//   A        enter highlighted folder
//   X        go up one level  (at drive list: same as B)
//   B        cancel - pop scene immediately
//   Start    confirm current directory as install path
//   Y        create new folder (keyboard)
//   White    rename selected folder (keyboard, pre-filled)
//   Black    delete selected folder (confirmation dialog)
//   D-Pad    navigate / alpha-jump (auto-repeat)
//   LT / RT  scroll up / down (variable speed)
// ---------------------------------------------------------------------------
class InstallPathScene : public Scene {
public:
  explicit InstallPathScene(InstallPathConfirmedCb callback = nullptr,
      void *userData = nullptr,
      const char *title = nullptr);
  virtual ~InstallPathScene();
  virtual void Render();
  virtual void Update();

  std::string GetChosenPath() const { return mChosenPath; }

private:
  // ---- Rendering ----
  void RenderHeader();
  void RenderFooter();
  void DrawFooterControl(float &x, float footerY, const char *iconName,
      const char *label);
  void RenderList();
  void RenderDeleteConfirm();

  // ---- Browse logic ----
  void NavigateInto();
  void NavigateUp();
  void ConfirmCurrent();
  void RefreshList(const std::string &selectName = "");

  // ---- Keyboard result handling ----
  enum KbPurpose { KbNone,
    KbCreate,
    KbRename };
  KbPurpose mKbPurpose;
  void HandleKeyboardResult();

  // ---- Delete logic ----
  void DeleteOpen();
  void DeleteConfirm();
  void RenderDeleteProgress();
  static DWORD WINAPI DeleteThreadProc(LPVOID param);
  static bool DeleteRecursive(const std::string &path,
      volatile bool *pCancel,
      volatile int32_t *pCurrent, volatile int32_t *pTotal,
      char *pCurrentName, int32_t currentNameSize);
  static int32_t CountFiles(const std::string &path);

  // ---- Helpers ----
  std::string GetDisplayName(int32_t index) const;
  char GetKeyChar(const std::string &name) const;
  int32_t JumpToNextLetter(int32_t startIdx, int direction) const;

  // ---- Browse state ----
  bool mInitialized;
  std::string mCurrentPath;
  int32_t mSelectedIndex;
  std::vector<FileInfoDetail> mItems;
  std::string mChosenPath;

  // ---- Auto-repeat timers ----
  DWORD mRepeatNext_Down;
  DWORD mRepeatNext_Up;
  DWORD mRepeatNext_Left;
  DWORD mRepeatNext_Right;
  DWORD mRepeatNext_LT;
  DWORD mRepeatNext_RT;

  // ---- Shared keyboard widget ----
  OnScreenKeyboard mKeyboard;

  // ---- Delete confirm state ----
  bool mDeleteOpen;
  std::string mDeletePath;
  std::string mDeleteName;

  // ---- Delete progress state ----
  bool mDeleting;
  volatile bool mDeleteCancelRequested;
  volatile int32_t mDeleteCurrent;
  volatile int32_t mDeleteTotal;
  char mDeleteCurrentName[256];
  HANDLE mDeleteThread;

  // ---- Callback ----
  InstallPathConfirmedCb mCallback;
  void *mUserData;
  std::string mTitle;
};