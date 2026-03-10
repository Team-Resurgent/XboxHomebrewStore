//=============================================================================
// ImageDownloader.h - Threaded queue for cover/screenshot image downloads
//=============================================================================

#pragma once

#include "Main.h"
#include <set>

enum ImageDownloadType { IMAGE_COVER,
  IMAGE_SCREENSHOT };

class ImageDownloader {
public:
  ImageDownloader();
  ~ImageDownloader();

  void Queue(D3DTexture **pOutTexture, const std::string appId,
      ImageDownloadType type);
  void WarmCache(const std::string appId,
      ImageDownloadType type); // download to disk only, no texture
  void CancelAll();
  bool HasPendingWork() const;

  static std::string GetCoverCachePath(const std::string appId);
  static bool IsCoverCached(const std::string appId);
  static std::string GetScreenshotCachePath(const std::string appId);
  static bool IsScreenshotCached(const std::string appId);
  static int32_t GetCachedCoverCount(); // number of covers currently on disk

private:
  struct Request {
    D3DTexture **pOutTexture;
    std::string appId;
    ImageDownloadType type;
  };

  static DWORD WINAPI ThreadProc(LPVOID param);
  void WorkerLoop();

  std::deque<Request> m_queue;
  CRITICAL_SECTION m_queueLock;
  HANDLE m_thread;
  volatile bool m_quit;
  volatile bool m_cancelRequested;
  volatile bool m_busy;
  std::set<std::string> m_failed;
};