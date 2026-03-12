//=============================================================================
// MetaCache.cpp -- Streaming per-category metadata cache
//=============================================================================

#include "MetaCache.h"
#include "Debug.h"
#include "FileSystem.h"
#include "String.h"
#include "WebManager.h"

#define META_CACHE_DIR        "T:\\Cache\\Meta"
#define META_FETCH_SIZE       100
#define META_MAGIC            0x4D455441  // 'META'
#define META_VERSION          3
#define META_ITEMCOUNT_OFFSET 8

// File format:
//   uint32_t magic         (offset 0)
//   uint32_t version       (offset 4)
//   uint32_t itemCount     (offset 8) -- written as 0, patched at end
//   per item:
//     uint32_t idLen   + id bytes (XOR obfuscated)
//     uint32_t nameLen + name chars
//     uint32_t authLen + author chars
//     uint32_t catLen  + category chars
//     uint32_t descLen + description chars
//     uint32_t lvLen   + latestVersion chars
//     int32_t  state

// ==========================================================================
// Statics
// ==========================================================================

std::string MetaCache::mCachedCategory = "";
std::vector<AppItem> MetaCache::mCachedItems;
bool MetaCache::mCacheReady = false;
volatile LONG MetaCache::mFetchCancel = 0;
HANDLE MetaCache::mFetchThread = NULL;
CRITICAL_SECTION MetaCache::mCacheLock;
volatile LONG MetaCache::mFetchFailed = 0;
volatile LONG MetaCache::mStreamedCount = 0;
volatile LONG MetaCache::mTotalCount = 0;

// ==========================================================================
// CRC32
// ==========================================================================

static uint32_t CalcCRC32(const void *data, size_t size) {
  static uint32_t s_table[256];
  static int32_t s_init = 0;
  if (!s_init) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int32_t k = 0; k < 8; k++) {
        c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
      }
      s_table[i] = c;
    }
    s_init = 1;
  }
  uint32_t crc = 0xFFFFFFFFU;
  const uint8_t *p = (const uint8_t *)data;
  for (size_t i = 0; i < size; i++) {
    crc = s_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFU;
}

// ==========================================================================
// ID obfuscation -- XOR with 'XBOX', symmetric
// ==========================================================================

static void ObfuscateId(std::string &s) {
  static const uint8_t key[] = { 0x58, 0x42, 0x4F, 0x58 };
  for (size_t i = 0; i < s.size(); i++) {
    s[i] = (char)((uint8_t)s[i] ^ key[i % 4]);
  }
}

// ==========================================================================
// Low-level file I/O helpers
// ==========================================================================

static bool WriteU32(uint32_t handle, uint32_t value) {
  uint32_t written = 0;
  return FileSystem::FileWrite(handle, (char *)&value, sizeof(uint32_t), written)
      && written == sizeof(uint32_t);
}

static bool WriteI32(uint32_t handle, int32_t value) {
  uint32_t written = 0;
  return FileSystem::FileWrite(handle, (char *)&value, sizeof(int32_t), written)
      && written == sizeof(int32_t);
}

static bool WriteStr(uint32_t handle, const std::string &s) {
  uint32_t len = (uint32_t)s.size();
  uint32_t written = 0;
  if (!FileSystem::FileWrite(handle, (char *)&len, sizeof(uint32_t), written)
      || written != sizeof(uint32_t)) {
    return false;
  }
  if (len == 0) return true;
  written = 0;
  return FileSystem::FileWrite(handle, (char *)s.c_str(), len, written)
      && written == len;
}

static bool ReadU32(uint32_t handle, uint32_t &value) {
  uint32_t read = 0;
  return FileSystem::FileRead(handle, (char *)&value, sizeof(uint32_t), read)
      && read == sizeof(uint32_t);
}

static bool ReadI32(uint32_t handle, int32_t &value) {
  uint32_t read = 0;
  return FileSystem::FileRead(handle, (char *)&value, sizeof(int32_t), read)
      && read == sizeof(int32_t);
}

static bool ReadStr(uint32_t handle, std::string &s) {
  uint32_t len = 0;
  if (!ReadU32(handle, len)) return false;
  if (len == 0) { s = ""; return true; }
  if (len > 8192) return false;
  char *buf = new char[len + 1];
  uint32_t read = 0;
  bool ok = FileSystem::FileRead(handle, buf, len, read) && read == len;
  if (ok) { buf[len] = '\0'; s = std::string(buf, len); }
  delete[] buf;
  return ok;
}

// ==========================================================================
// Path helpers
// ==========================================================================

uint32_t MetaCache::CategoryCRC(const std::string &category) {
  if (category.empty()) return 0x00000000U;
  return CalcCRC32(category.c_str(), category.size());
}

std::string MetaCache::CachePath(const std::string &category) {
  return String::Format("%s\\%08X.bin", META_CACHE_DIR, CategoryCRC(category));
}

// ==========================================================================
// Streaming write helpers
// ==========================================================================

bool MetaCache::OpenForWrite(const std::string &path, uint32_t &handle) {
  if (!FileSystem::FileOpen(path, FileModeWrite, handle)) {
    Debug::Print("MetaCache: failed to open %s for write\n", path.c_str());
    return false;
  }
  bool ok = WriteU32(handle, META_MAGIC)
      && WriteU32(handle, META_VERSION)
      && WriteU32(handle, 0); // itemCount placeholder
  if (!ok) {
    FileSystem::FileClose(handle);
    FileSystem::FileDelete(path);
  }
  return ok;
}

bool MetaCache::AppendItems(uint32_t handle, const std::vector<AppItem> &items) {
  bool ok = true;
  for (int32_t i = 0; i < (int32_t)items.size() && ok; i++) {
    const AppItem &item = items[i];
    std::string obfId = item.id;
    ObfuscateId(obfId);
    ok = ok && WriteStr(handle, obfId)
            && WriteStr(handle, item.name)
            && WriteStr(handle, item.author)
            && WriteStr(handle, item.category)
            && WriteStr(handle, item.description)
            && WriteStr(handle, item.latestVersion)
            && WriteI32(handle, item.state);
  }
  return ok;
}

bool MetaCache::FinaliseWrite(uint32_t handle, int32_t totalItems) {
  bool ok = FileSystem::FileSeek(handle, FileSeekModeStart, META_ITEMCOUNT_OFFSET)
      && WriteU32(handle, (uint32_t)totalItems);
  FileSystem::FileClose(handle);
  return ok;
}

// ==========================================================================
// LoadFromDisk -- called ONLY from fetch thread after write is complete
// Holds lock while updating shared state
// ==========================================================================

bool MetaCache::LoadFromDisk(const std::string &category) {
  std::string path = CachePath(category);

  uint32_t handle = 0;
  if (!FileSystem::FileOpen(path, FileModeRead, handle)) {
    return false;
  }

  uint32_t magic = 0, version = 0, itemCount = 0;
  bool ok = ReadU32(handle, magic)
      && ReadU32(handle, version)
      && ReadU32(handle, itemCount);

  if (!ok || magic != META_MAGIC || version != META_VERSION || itemCount == 0) {
    FileSystem::FileClose(handle);
    Debug::Print("MetaCache: bad/empty header in %s\n", path.c_str());
    FileSystem::FileDelete(path);
    return false;
  }

  // Read all items into a local buffer first -- don't touch shared state yet
  std::vector<AppItem> loaded;
  loaded.reserve(itemCount);

  for (uint32_t i = 0; i < itemCount && ok; i++) {
    AppItem item;
    int32_t state = 0;
    ok = ok
        && ReadStr(handle, item.id)
        && ReadStr(handle, item.name)
        && ReadStr(handle, item.author)
        && ReadStr(handle, item.category)
        && ReadStr(handle, item.description)
        && ReadStr(handle, item.latestVersion)
        && ReadI32(handle, state);
    if (ok) {
      ObfuscateId(item.id);
      item.state = state;
      loaded.push_back(item);
    }
  }

  FileSystem::FileClose(handle);

  if (!ok || loaded.empty()) {
    Debug::Print("MetaCache: read error, deleting file\n");
    FileSystem::FileDelete(path);
    return false;
  }

  // Now swap into shared state under lock
  EnterCriticalSection(&mCacheLock);
  mCachedItems.swap(loaded);
  mCachedCategory = category;
  mCacheReady = true;
  LeaveCriticalSection(&mCacheLock);

  Debug::Print("MetaCache: loaded %d items into memory\n",
      (int32_t)mCachedItems.size());
  return true;
}

// ==========================================================================
// Init -- wipes cache on startup, inits lock
// ==========================================================================

void MetaCache::Init() {
  InitializeCriticalSection(&mCacheLock);

  EnterCriticalSection(&mCacheLock);
  mCachedCategory = "";
  mCachedItems.clear();
  mCacheReady = false;
  LeaveCriticalSection(&mCacheLock);

  bool exists = false;
  FileSystem::DirectoryExists(META_CACHE_DIR, exists);
  if (exists) {
    FileSystem::DirectoryDelete(META_CACHE_DIR, true);
    Debug::Print("MetaCache: wiped cache on startup\n");
  }
  FileSystem::DirectoryCreate(META_CACHE_DIR);
}

// ==========================================================================
// PurgeAll
// ==========================================================================

void MetaCache::PurgeAll() {
  bool exists = false;
  FileSystem::DirectoryExists(META_CACHE_DIR, exists);
  if (exists) {
    FileSystem::DirectoryDelete(META_CACHE_DIR, true);
    FileSystem::DirectoryCreate(META_CACHE_DIR);
  }
  EnterCriticalSection(&mCacheLock);
  mCachedItems.clear();
  mCachedCategory = "";
  mCacheReady = false;
  LeaveCriticalSection(&mCacheLock);
  Debug::Print("MetaCache: purged all\n");
}

// ==========================================================================
// TryGetPage -- main thread only, returns false if fetch still running
// ==========================================================================

bool MetaCache::TryGetPage(const std::string &category, int32_t offset,
    int32_t count, AppsResponse &result) {
  result.items.clear();

  EnterCriticalSection(&mCacheLock);

  // Only serve if cache is fully ready for this category
  // If fetch thread is still building it, mCacheReady will be false -- fall through to API
  if (!mCacheReady || mCachedCategory != category) {
    LeaveCriticalSection(&mCacheLock);
    return false;
  }

  int32_t total = (int32_t)mCachedItems.size();
  int32_t end = offset + count;
  if (end > total) end = total;

  for (int32_t i = offset; i < end; i++) {
    result.items.push_back(mCachedItems[i]);
  }

  LeaveCriticalSection(&mCacheLock);

  return !result.items.empty();
}

// ==========================================================================
// Background fetch
// ==========================================================================

struct FetchParams {
  std::string category;
  int32_t totalCount;
};

void MetaCache::StopFetch() {
  InterlockedExchange((LPLONG)&mFetchCancel, 1);
  if (mFetchThread == NULL) {
    InterlockedExchange((LPLONG)&mFetchCancel, 0);
  }
}

bool MetaCache::IsReady() {
  return mCacheReady;
}

bool MetaCache::IsFailed() {
  return mFetchFailed != 0;
}

int32_t MetaCache::GetStreamedCount() {
  return (int32_t)mStreamedCount;
}

int32_t MetaCache::GetTotalCount() {
  return (int32_t)mTotalCount;
}

void MetaCache::StartFetch(const std::string &category, int32_t totalCount) {
  StopFetch();

  // Reset progress and failure state
  InterlockedExchange((LPLONG)&mFetchFailed, 0);
  InterlockedExchange((LPLONG)&mStreamedCount, 0);
  InterlockedExchange((LPLONG)&mTotalCount, (LONG)totalCount);

  // Clear memory cache for this category -- file was just purged
  EnterCriticalSection(&mCacheLock);
  if (mCachedCategory == category) {
    mCachedItems.clear();
    mCachedCategory = "";
    mCacheReady = false;
  }
  LeaveCriticalSection(&mCacheLock);

  FetchParams *params = new FetchParams();
  params->category = category;
  params->totalCount = totalCount;

  InterlockedExchange((LPLONG)&mFetchCancel, 0);

  HANDLE h = CreateThread(NULL, 0, FetchThreadProc, params, 0, NULL);
  mFetchThread = h;
  if (h != NULL) {
    CloseHandle(h);
  } else {
    delete params;
    Debug::Print("MetaCache: CreateThread failed\n");
  }
}

DWORD WINAPI MetaCache::FetchThreadProc(LPVOID param) {
  FetchParams *p = (FetchParams *)param;
  std::string category = p->category;
  int32_t totalCount = p->totalCount;
  delete p;

  Debug::Print("MetaCache: fetch started category='%s' total=%d\n",
      category.c_str(), totalCount);

  std::string path = CachePath(category);

  uint32_t writeHandle = 0;
  if (!MetaCache::OpenForWrite(path, writeHandle)) {
    InterlockedExchange((LPLONG)&mFetchFailed, 1);
    mFetchThread = NULL;
    return 0;
  }

  int32_t offset = 0;
  int32_t totalWritten = 0;
  bool fetchOk = true;

  while (mFetchCancel == 0) {
    AppsResponse page;

    // Retry each page up to 3 times before giving up
    bool pageOk = false;
    int32_t attempts = 0;
    while (attempts < 3 && mFetchCancel == 0) {
      if (WebManager::TryGetApps(page, offset, META_FETCH_SIZE, category, "")) {
        pageOk = true;
        break;
      }
      attempts++;
      if (attempts < 3 && mFetchCancel == 0) {
        Debug::Print("MetaCache: page fetch failed at offset=%d, retry %d/3\n",
            offset, attempts);
        Sleep(2000);
      }
    }

    if (!pageOk) {
      Debug::Print("MetaCache: API fetch failed at offset=%d after 3 attempts\n", offset);
      fetchOk = false;
      break;
    }

    int32_t pageCount = (int32_t)page.items.size();
    if (pageCount == 0) {
      break;
    }

    if (!MetaCache::AppendItems(writeHandle, page.items)) {
      Debug::Print("MetaCache: write failed at offset=%d\n", offset);
      fetchOk = false;
      break;
    }

    totalWritten += pageCount;
    offset += pageCount;
    InterlockedExchange((LPLONG)&mStreamedCount, (LONG)totalWritten);
    Debug::Print("MetaCache: streamed %d/%d items\n", totalWritten, totalCount);

    page.items.clear();

    if (pageCount < META_FETCH_SIZE) {
      break;
    }
  }

  if (mFetchCancel != 0) {
    FileSystem::FileClose(writeHandle);
    FileSystem::FileDelete(path);
    Debug::Print("MetaCache: fetch cancelled, partial file deleted\n");
    InterlockedExchange((LPLONG)&mFetchCancel, 0);
    mFetchThread = NULL;
    return 0;
  }

  if (!fetchOk || totalWritten == 0) {
    FileSystem::FileClose(writeHandle);
    FileSystem::FileDelete(path);
    Debug::Print("MetaCache: fetch failed, file deleted\n");
    InterlockedExchange((LPLONG)&mFetchFailed, 1);
    mFetchThread = NULL;
    return 0;
  }

  // Patch itemCount and close -- file is now valid
  if (!MetaCache::FinaliseWrite(writeHandle, totalWritten)) {
    FileSystem::FileDelete(path);
    Debug::Print("MetaCache: finalise failed\n");
    InterlockedExchange((LPLONG)&mFetchFailed, 1);
    mFetchThread = NULL;
    return 0;
  }

  Debug::Print("MetaCache: fetch complete, %d items written\n", totalWritten);

  // Load into memory under lock so main thread can serve from it immediately
  MetaCache::LoadFromDisk(category);

  mFetchThread = NULL;
  return 0;
}