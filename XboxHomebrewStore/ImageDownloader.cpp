//=============================================================================
// ImageDownloader.cpp - Threaded cover download
// Server returns DDS directly -- no client-side conversion needed.
// Pipeline:
//   DownloadThread -- HTTP fetch, saves .dds to disk
//   Main thread    -- TextureHelper::LoadFromFile(.dds): CreateTexture+LockRect+memcpy
//=============================================================================
#include "ImageDownloader.h"
#include "Context.h"
#include "StoreList.h"
#include "StoreManager.h"
#include "Defines.h"
#include "WebManager.h"
#include "Debug.h"
#include "FileSystem.h"
#include "String.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static uint32_t CRC32(const void *data, size_t size) {
  static uint32_t s_table[256];
  static int32_t  s_tableInit = 0;
  if (!s_tableInit) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int32_t k = 0; k < 8; k++)
        c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
      s_table[i] = c;
    }
    s_tableInit = 1;
  }
  uint32_t crc = 0xFFFFFFFFU;
  const uint8_t *p = (const uint8_t *)data;
  for (size_t i = 0; i < size; i++)
    crc = s_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFU;
}

static std::string DdsCachePathFor(const std::string appId, ImageDownloadType type) {
  uint32_t    crc  = CRC32(appId.c_str(), appId.size());
  std::string root = StoreList::GetActiveCacheRoot();
  if (type == IMAGE_COVER)
    return String::Format("%s\\Covers\\%08X.dds", root.c_str(), crc);
  return String::Format("%s\\Screenshots\\%08X.dds", root.c_str(), crc);
}

static bool FileExists(const char *path) {
  DWORD att = GetFileAttributesA(path);
  if (att == (DWORD)-1)              return false;
  if (att & FILE_ATTRIBUTE_DIRECTORY) return false;
  return true;
}

// A file is "available" (fully written) unless it still has the ARCHIVE flag
// set by the download thread while the transfer is in progress.
static bool FileExistsAndAvailable(const char *path) {
  DWORD att = GetFileAttributesA(path);
  if (att == (DWORD)-1)              return false;
  if (att & FILE_ATTRIBUTE_DIRECTORY) return false;
  if (att & FILE_ATTRIBUTE_ARCHIVE)   return false;
  return true;
}

// ---------------------------------------------------------------------------
// Stable cover count
// ---------------------------------------------------------------------------
static volatile LONG s_cachedCoverCount = 0;

// ---------------------------------------------------------------------------
// Cache eviction helpers
// ---------------------------------------------------------------------------
static void CollectFileWithTime(const char *dir,
    std::vector<std::pair<std::string, ULONGLONG> > *out) {
  std::string pattern = std::string(dir) + "\\*";
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    ULARGE_INTEGER u;
    u.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
    u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
    out->push_back(std::make_pair(
        std::string(dir) + "\\" + fd.cFileName, u.QuadPart));
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}

static int32_t CountCacheFiles() {
  std::string root = StoreList::GetActiveCacheRoot();
  std::vector<std::pair<std::string, ULONGLONG> > files;
  CollectFileWithTime((root + "\\Covers").c_str(),      &files);
  CollectFileWithTime((root + "\\Screenshots").c_str(), &files);
  return (int32_t)files.size();
}

static std::string FindOldestCacheFile() {
  std::string root = StoreList::GetActiveCacheRoot();
  std::vector<std::pair<std::string, ULONGLONG> > files;
  CollectFileWithTime((root + "\\Covers").c_str(),      &files);
  CollectFileWithTime((root + "\\Screenshots").c_str(), &files);
  if (files.empty()) return std::string();
  size_t oldest = 0;
  for (size_t i = 1; i < files.size(); i++)
    if (files[i].second < files[oldest].second) oldest = i;
  return files[oldest].first;
}

static void EnforceCacheLimit() {
  while (CountCacheFiles() >= STORE_CACHE_FILE_LIMIT) {
    std::string old = FindOldestCacheFile();
    if (old.empty()) break;
    DeleteFileA(old.c_str());
  }
}

// ---------------------------------------------------------------------------
// Public static helpers
// ---------------------------------------------------------------------------
void ImageDownloader::ResetCachedCoverCount() {
  std::string root = StoreList::GetActiveCacheRoot();
  std::vector<std::pair<std::string, ULONGLONG> > files;
  CollectFileWithTime((root + "\\Covers").c_str(), &files);
  InterlockedExchange((LPLONG)&s_cachedCoverCount, (LONG)files.size());
}

int32_t ImageDownloader::GetCachedCoverCount() {
  return (int32_t)s_cachedCoverCount;
}

int32_t ImageDownloader::GetFailedCoverCount() {
  std::string root = StoreList::GetActiveCacheRoot();
  std::vector<std::pair<std::string, ULONGLONG> > files;
  CollectFileWithTime((root + "\\Covers\\Failed").c_str(), &files);
  CollectFileWithTime((root + "\\Screenshots\\Failed").c_str(), &files);
  return (int32_t)files.size();
}

void ImageDownloader::ClearFailedCovers() {
  std::string root = StoreList::GetActiveCacheRoot();
  std::string coversFailed      = root + "\\Covers\\Failed";
  std::string screenshotsFailed = root + "\\Screenshots\\Failed";
  FileSystem::DirectoryDelete(coversFailed.c_str(), true);
  FileSystem::DirectoryDelete(screenshotsFailed.c_str(), true);
  FileSystem::DirectoryCreate(coversFailed.c_str());
  FileSystem::DirectoryCreate(screenshotsFailed.c_str());
  // Reset idle warmer so it re-scans and picks up the newly cleared covers
  StoreManager::InvalidateCovers();
  Debug::Print("ImageDownloader: cleared failed covers, idle warmer reset\n");
}

std::string ImageDownloader::GetCoverCachePath(const std::string appId) {
  std::string root = StoreList::GetActiveCacheRoot();
  uint32_t    crc  = CRC32(appId.c_str(), appId.size());
  return String::Format("%s\\Covers\\%08X.dds", root.c_str(), crc);
}

bool ImageDownloader::IsCoverCached(const std::string appId) {
  std::string path = GetCoverCachePath(appId);
  if (!FileExistsAndAvailable(path.c_str())) return false;
  // Reject files too small to be a valid DDS (128 byte header minimum)
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(path.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return false;
  FindClose(h);
  if (fd.nFileSizeLow < 128) {
    DeleteFileA(path.c_str());
    return false;
  }
  return true;
}

bool ImageDownloader::IsCoverFailed(const std::string appId) {
  std::string ddsPath  = GetCoverCachePath(appId);
  std::string failDir  = ddsPath.substr(0, ddsPath.rfind("\\") + 1) + "Failed";
  std::string failName = ddsPath.substr(ddsPath.rfind("\\") + 1);
  if (failName.size() > 4) failName = failName.substr(0, failName.size() - 4) + ".fail";
  return FileExists((failDir + "\\" + failName).c_str());
}

void ImageDownloader::ClearFailedCover(const std::string appId) {
  std::string ddsPath  = GetCoverCachePath(appId);
  std::string failDir  = ddsPath.substr(0, ddsPath.rfind("\\") + 1) + "Failed";
  std::string failName = ddsPath.substr(ddsPath.rfind("\\") + 1);
  if (failName.size() > 4) failName = failName.substr(0, failName.size() - 4) + ".fail";
  DeleteFileA((failDir + "\\" + failName).c_str());
}

bool ImageDownloader::IsScreenshotFailed(const std::string appId) {
  std::string ddsPath  = GetScreenshotCachePath(appId);
  std::string failDir  = ddsPath.substr(0, ddsPath.rfind("\\") + 1) + "Failed";
  std::string failName = ddsPath.substr(ddsPath.rfind("\\") + 1);
  if (failName.size() > 4) failName = failName.substr(0, failName.size() - 4) + ".fail";
  return FileExists((failDir + "\\" + failName).c_str());
}

void ImageDownloader::ClearFailedScreenshot(const std::string appId) {
  std::string ddsPath  = GetScreenshotCachePath(appId);
  std::string failDir  = ddsPath.substr(0, ddsPath.rfind("\\") + 1) + "Failed";
  std::string failName = ddsPath.substr(ddsPath.rfind("\\") + 1);
  if (failName.size() > 4) failName = failName.substr(0, failName.size() - 4) + ".fail";
  DeleteFileA((failDir + "\\" + failName).c_str());
}

std::string ImageDownloader::GetScreenshotCachePath(const std::string appId) {
  return DdsCachePathFor(appId, IMAGE_SCREENSHOT);
}

bool ImageDownloader::IsScreenshotCached(const std::string appId) {
  return FileExistsAndAvailable(GetScreenshotCachePath(appId).c_str());
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
ImageDownloader::ImageDownloader()
    : m_downloadThread(NULL)
    , m_quit(false)
    , m_cancelRequested(false)
    , m_busy(false)
    , m_busyType(IMAGE_COVER) {
  InitializeCriticalSection(&m_queueLock);
  m_downloadThread = CreateThread(NULL, 0, DownloadThreadProc, this, 0, NULL);
}

ImageDownloader::~ImageDownloader() {
  m_quit = true;
  if (m_downloadThread) {
    WaitForSingleObject(m_downloadThread, INFINITE);
    CloseHandle(m_downloadThread);
  }
  DeleteCriticalSection(&m_queueLock);
}

// ---------------------------------------------------------------------------
// Queue / WarmCache / CancelAll / FlushQueue
// ---------------------------------------------------------------------------
void ImageDownloader::Queue(D3DTexture **pOutTexture, const std::string appId,
    ImageDownloadType type) {
  if (!pOutTexture || appId.empty()) return;
  EnterCriticalSection(&m_queueLock);
  if (m_busy && m_busyAppId == appId && m_busyType == type) {
    LeaveCriticalSection(&m_queueLock);
    return;
  }
  for (uint32_t i = 0; i < m_queue.size(); i++) {
    if (m_queue[i].appId == appId && m_queue[i].type == type) {
      LeaveCriticalSection(&m_queueLock);
      return;
    }
  }
  Request r;
  r.pOutTexture = pOutTexture;
  r.appId       = appId;
  r.type        = type;
  m_queue.push_back(r);
  LeaveCriticalSection(&m_queueLock);
}

void ImageDownloader::WarmCache(const std::string appId, ImageDownloadType type) {
  if (appId.empty()) return;
  if (type == IMAGE_COVER      && IsCoverCached(appId))      return;
  if (type == IMAGE_SCREENSHOT && IsScreenshotCached(appId)) return;
  EnterCriticalSection(&m_queueLock);
  if (m_busy && m_busyAppId == appId && m_busyType == type) {
    LeaveCriticalSection(&m_queueLock);
    return;
  }
  for (uint32_t i = 0; i < m_queue.size(); i++) {
    if (m_queue[i].appId == appId && m_queue[i].type == type) {
      LeaveCriticalSection(&m_queueLock);
      return;
    }
  }
  Request r;
  r.pOutTexture = NULL;
  r.appId       = appId;
  r.type        = type;
  m_queue.push_back(r);
  LeaveCriticalSection(&m_queueLock);
}

void ImageDownloader::CancelAll() {
  m_cancelRequested = true;
  EnterCriticalSection(&m_queueLock);
  m_queue.clear();
  LeaveCriticalSection(&m_queueLock);
  // NOTE: m_failed is intentionally NOT cleared here.
  // Scrolling should not cause 404 covers to be retried.
}

void ImageDownloader::FlushQueue() {
  EnterCriticalSection(&m_queueLock);
  m_queue.clear();
  LeaveCriticalSection(&m_queueLock);
}

void ImageDownloader::SetVisibleQueue(D3DTexture **pOutTextures[],
    const std::string appIds[], int32_t count) {
  EnterCriticalSection(&m_queueLock);
  m_queue.clear();
  bool inFlightStillNeeded = false;
  if (m_busy && !m_busyAppId.empty()) {
    for (int32_t i = 0; i < count; i++) {
      if (appIds[i] == m_busyAppId) { inFlightStillNeeded = true; break; }
    }
  }
  if (m_busy && !inFlightStillNeeded)
    m_cancelRequested = true;
  for (int32_t i = 0; i < count; i++) {
    if (appIds[i].empty()) continue;
    if (m_busy && m_busyAppId == appIds[i] && m_busyType == IMAGE_COVER) continue;
    Request r;
    r.pOutTexture = pOutTextures[i];
    r.appId       = appIds[i];
    r.type        = IMAGE_COVER;
    m_queue.push_back(r);
  }
  LeaveCriticalSection(&m_queueLock);
}

bool ImageDownloader::HasPendingWork() const {
  EnterCriticalSection(const_cast<CRITICAL_SECTION *>(&m_queueLock));
  bool pending = !m_queue.empty();
  LeaveCriticalSection(const_cast<CRITICAL_SECTION *>(&m_queueLock));
  return pending || m_busy;
}

// ---------------------------------------------------------------------------
// Download thread -- fetches DDS from server, saves to disk
// ---------------------------------------------------------------------------
DWORD WINAPI ImageDownloader::DownloadThreadProc(LPVOID param) {
  ImageDownloader *p = (ImageDownloader *)param;
  if (p) p->DownloadLoop();
  return 0;
}

void ImageDownloader::DownloadLoop() {
  while (!m_quit) {
    Request req;
    bool haveRequest = false;
    EnterCriticalSection(&m_queueLock);
    if (!m_queue.empty()) {
      req          = m_queue.front();
      m_queue.pop_front();
      haveRequest  = true;
      m_cancelRequested = false;
    }
    LeaveCriticalSection(&m_queueLock);

    if (!haveRequest) { Sleep(10); continue; }

    m_busy = true;
    std::string ddsPath = DdsCachePathFor(req.appId, req.type);

    EnterCriticalSection(&m_queueLock);
    m_busyAppId = req.appId;
    m_busyType  = req.type;
    LeaveCriticalSection(&m_queueLock);

    // A .fail marker in the Failed subfolder persists 404s across scrolls
    // and restarts. Cleared via "Retry Failed Covers" in Settings, or
    // when the full cache is wiped.
    // e.g. Covers\Failed\AABB1234.fail
    std::string failDir  = ddsPath.substr(0, ddsPath.rfind("\\") + 1) + "Failed";
    std::string failName = ddsPath.substr(ddsPath.rfind("\\") + 1);
    // Replace .dds extension with .fail
    if (failName.size() > 4) failName = failName.substr(0, failName.size() - 4) + ".fail";
    std::string failPath = failDir + "\\" + failName;

    bool alreadyCached    = FileExistsAndAvailable(ddsPath.c_str());
    bool previouslyFailed = FileExists(failPath.c_str());

    if (!alreadyCached && !previouslyFailed) {
      EnforceCacheLimit();

      // Mark as in-progress so FileExistsAndAvailable() rejects partial file
      {
        FILE *touch = fopen(ddsPath.c_str(), "wb");
        if (touch) fclose(touch);
      }
      SetFileAttributesA(ddsPath.c_str(), FILE_ATTRIBUTE_ARCHIVE);

      bool ok = false;
      if (req.type == IMAGE_COVER) {
        ok = WebManager::TryDownloadCover(
            req.appId, 288, 408, ddsPath, NULL, NULL, &m_cancelRequested);
      } else {
        ok = WebManager::TryDownloadScreenshot(
            req.appId, 640, 360, ddsPath, NULL, NULL, &m_cancelRequested);
      }

      if (m_cancelRequested || m_quit) {
        DeleteFileA(ddsPath.c_str());
        EnterCriticalSection(&m_queueLock);
        m_busyAppId = "";
        LeaveCriticalSection(&m_queueLock);
        m_busy = false;
        continue;
      }

      if (!ok) {
        DeleteFileA(ddsPath.c_str());
        // Write .fail marker -- prevents retrying until explicitly cleared.
        // Covers: never retried unless user hits "Retry Failed Covers" in Settings.
        // Screenshots: retried based on RetryFailedOnView setting in VersionScene.
        FILE *ff = fopen(failPath.c_str(), "wb");
        if (ff) fclose(ff);
        EnterCriticalSection(&m_queueLock);
        m_busyAppId = "";
        LeaveCriticalSection(&m_queueLock);
        m_busy = false;
        continue;
      }

      // Download complete -- clear ARCHIVE flag so main thread can load it
      SetFileAttributesA(ddsPath.c_str(), FILE_ATTRIBUTE_NORMAL);
      InterlockedIncrement((LPLONG)&s_cachedCoverCount);
      Debug::Print("ImageDownloader: saved %s\n", ddsPath.c_str());
    }

    EnterCriticalSection(&m_queueLock);
    m_busyAppId = "";
    LeaveCriticalSection(&m_queueLock);
    m_busy = false;
  }
}