#pragma once

#include "Scene.h"

#include "..\AppSettings.h"
#include "..\ImageDownloader.h"
#include "..\Main.h"
#include "..\StoreManager.h"
#include "..\WebManager.h"

class VersionScene : public Scene {
public:
  explicit VersionScene(const StoreVersions &storeVersions);
  virtual ~VersionScene();
  virtual void Render();
  virtual void Update();
  virtual void OnResume();

  // Called by InstallPathScene callback
  void SetPendingInstallPath(const std::string &path);

private:
  void RenderHeader();
  void RenderFooter();
  void DrawFooterControl(float &x, float footerY, const char *iconName,
      const char *label);
  void RenderVersionSidebar();
  void RenderListView();
  void RenderDownloadOverlay();
  void RenderCheckingLinksOverlay();
  void RenderFailedOverlay();
  void RenderAfterInstallDialog();

  void BrowseInstallPath();
  void RefreshCoverArt();
  void StartDownload();
  void HandleAfterInstall();

  static void DownloadProgressCb(uint32_t dlNow, uint32_t dlTotal,
      void *userData);
  static bool UnpackProgressCb(int currentFile, int totalFiles,
      const char *currentFileName, void *userData);
  static DWORD WINAPI DownloadThreadProc(LPVOID param);
  static DWORD WINAPI SizeFetchThreadProc(LPVOID param);

  ImageDownloader *mImageDownloader;
  StoreVersions mStoreVersions;

  bool mNeedsUpdate;
  bool mCoverQueued;
  bool mScreenshotQueued;
  bool mSideBarFocused;
  int32_t mHighlightedVersionIndex;
  int32_t mVersionIndex;

  float mListViewScrollOffset;
  float mListViewContentHeight;
  float mDescriptionHeight;
  float mChangeLogHeight;
  int32_t mLastMeasuredVersionIndex;

  // ---- Install path selection ----
  std::string mPendingInstallPath; // set by callback, cleared after use
  bool mWaitingForInstallPath;     // true while InstallPathScene is on stack

  // ---- Download / unpack ----
  bool mDownloading;
  bool mDownloadSucceeded;
  volatile bool mDownloadCancelRequested;
  volatile uint32_t mDownloadCurrent;
  volatile uint32_t mDownloadTotal;
  volatile int mProgressIndex;
  volatile int mProgressCount;
  bool mDownloadSuccess;
  bool mShowFailedOverlay;
  std::string mFailedMessage;
  bool mCheckingLinks;
  HANDLE mDownloadThread;
  bool mUnpacking;
  volatile bool mUnpackCancelRequested;
  volatile int mUnpackCurrent;
  volatile int mUnpackTotal;

  // Paths used by the download thread (set before thread launch)
  std::string mDownloadPath;
  std::string mInstallPath;

  // ---- After-install "Ask" dialog ----
  bool mShowAfterInstallDialog;

  // ---- Background URL size fetcher ----
  HANDLE mSizeFetchThread;
  volatile bool mSizeFetchDone;
};