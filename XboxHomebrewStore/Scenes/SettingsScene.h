#pragma once

#include "..\AppSettings.h"
#include "..\Main.h"
#include "..\StoreList.h"
#include "Scene.h"

// ---------------------------------------------------------------------------
// SettingsScene
//
// Seven rows:
//   0 - Active Store              (A = push StoreManagerScene)
//   1 - Download Path             (A = browse)
//   2 - Cache Location            (A = open popup picker: T: / D: / Custom)
//   3 - Keep Downloaded Files     (A = open popup picker)
//   4 - Show Cache Partitions     (A = toggle)
//   5 - Pre-cache Covers on Idle  (A = toggle)
//   6 - Clear Image Cache         (A = confirm dialog)
//
// B / Start  =  save & go back
// ---------------------------------------------------------------------------
class InstallPathScene;
class StoreManagerScene;

class SettingsScene : public Scene {
public:
  SettingsScene();
  virtual ~SettingsScene();
  virtual void Render();
  virtual void Update();
  virtual void OnResume();

  void SetDownloadPath(const std::string &path) { mDownloadPath = path; }
  void SetCachePath(const std::string &path) { mCachePath = path; }

private:
  void OpenPathBrowser();
  void OpenCachePathBrowser();
  void SaveAndPop();
  void RenderPicker();
  void RenderCacheLocationPicker();
  void RenderClearCacheConfirm();
  void RenderRetryFailedConfirm();

  int32_t mSelectedRow;

  std::string mDownloadPath;
  AfterInstallAction mAfterInstallAction;
  bool mShowCachePartitions;
  bool mPreCacheOnIdle;
  CacheLocation mCacheLocation;
  std::string mCachePath;

  // Popup picker state (after-install)
  bool mPickerOpen;
  int32_t mPickerSel;

  // Cache location picker state
  bool mCachePickerOpen;
  int32_t mCachePickerSel;

  // Clear cache confirm dialog
  bool mClearCacheConfirmOpen;
  bool mClearCacheDone;

  // Retry failed covers confirm dialog
  bool mRetryFailedConfirmOpen;
  bool mRetryFailedDone;

  // Body scroll
  float mScrollOffset;

  static const int ROW_COUNT = 8;
};