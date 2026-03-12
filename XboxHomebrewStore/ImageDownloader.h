//=============================================================================
// ImageDownloader.h - Threaded download + background JPG->DXT1 conversion
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
  static std::string GetCoverJpgPath(const std::string appId);
  static bool IsCoverCached(const std::string appId);
  static std::string GetScreenshotCachePath(const std::string appId);
  static bool IsScreenshotCached(const std::string appId);
  static int32_t GetCachedCoverCount();
  static void ResetCachedCoverCount(); // call once at startup after cache dir scan

private:
  // Download queue
  struct Request {
    D3DTexture **pOutTexture;
    std::string appId;
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
  std::string           m_busyAppId;   // appId currently being downloaded
  ImageDownloadType     m_busyType;    // type currently being downloaded
  std::set<std::string> m_failed;

  // Converter queue -- jpg paths pushed by download thread, consumed by converter thread
  static DWORD WINAPI ConverterThreadProc(LPVOID param);
  void ConverterLoop();

  std::deque<std::string> m_convertQueue;
  CRITICAL_SECTION        m_convertLock;
  HANDLE                  m_convertThread;
  volatile bool           m_convertBusy;
};