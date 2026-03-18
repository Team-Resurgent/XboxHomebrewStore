//=============================================================================
// ImageDownloader.h - Threaded cover download (server returns DDS directly)
//=============================================================================
#pragma once
#include "Main.h"
#include <set>
#include <deque>
#include <string>

enum ImageDownloadType { IMAGE_COVER, IMAGE_SCREENSHOT };

class ImageDownloader {
public:
  ImageDownloader();
  ~ImageDownloader();

  void Queue(D3DTexture **pOutTexture, const std::string appId,
      ImageDownloadType type);
  void SetVisibleQueue(D3DTexture **pOutTextures[], const std::string appIds[],
      int32_t count);
  void WarmCache(const std::string appId, ImageDownloadType type);
  void CancelAll();
  void FlushQueue();
  bool HasPendingWork() const;

  static std::string GetCoverCachePath(const std::string appId);
  static bool        IsCoverCached(const std::string appId);
  static bool        IsCoverFailed(const std::string appId);
  static void        ClearFailedCover(const std::string appId); // delete single .fail so it retries
  static std::string GetScreenshotCachePath(const std::string appId);
  static bool        IsScreenshotCached(const std::string appId);
  static int32_t     GetCachedCoverCount();
  static void        ResetCachedCoverCount();
  static int32_t     GetFailedCoverCount();
  static void        ClearFailedCovers();   // deletes Failed\ dirs so covers retry

private:
  struct Request {
    D3DTexture **pOutTexture;
    std::string  appId;
    ImageDownloadType type;
  };

  static DWORD WINAPI DownloadThreadProc(LPVOID param);
  void DownloadLoop();

  std::deque<Request>   m_queue;
  CRITICAL_SECTION      m_queueLock;
  HANDLE                m_downloadThread;
  volatile bool         m_quit;
  volatile bool         m_cancelRequested;
  volatile bool         m_busy;
  std::string           m_busyAppId;
  ImageDownloadType     m_busyType;
  std::set<std::string> m_failed;
};