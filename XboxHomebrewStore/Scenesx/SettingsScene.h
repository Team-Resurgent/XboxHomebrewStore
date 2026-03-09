#pragma once

#include "..\AppSettings.h"
#include "..\Main.h"
#include "..\StoreList.h"
#include "Scene.h"

// ---------------------------------------------------------------------------
// SettingsScene
//
// Six rows:
//   0 - Active Store              (A = push StoreManagerScene)
//   1 - Download Path             (A = browse)
//   2 - Keep Downloaded Files     (A = open popup picker)
//   3 - Show Cache Partitions     (A = toggle)
//   4 - Pre-cache Covers on Idle  (A = toggle)
//   5 - Clear Image Cache         (A = confirm dialog)
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

private:
  void OpenPathBrowser();
  void SaveAndPop();
  void RenderPicker();
  void RenderClearCacheConfirm();

  int32_t mSelectedRow;

  std::string mDownloadPath;
  AfterInstallAction mAfterInstallAction;
  bool mShowCachePartitions;
  bool mPreCacheOnIdle;

  // Popup picker state
  bool mPickerOpen;
  int32_t mPickerSel; // 0=Delete 1=Keep 2=Ask

  // Clear cache confirm dialog
  bool mClearCacheConfirmOpen;
  bool mClearCacheDone;

  // Body scroll
  float mScrollOffset;

  static const int ROW_COUNT = 6;
};
