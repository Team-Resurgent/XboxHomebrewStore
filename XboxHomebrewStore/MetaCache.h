#pragma once

#include "Main.h"
#include "Models.h"

class MetaCache {
public:
  static void Init();
  static void StartFetch(const std::string &category, int32_t totalCount);
  static void StopFetch();
  static bool IsReady();
  static bool IsFailed();
  static int32_t GetStreamedCount();
  static int32_t GetTotalCount();
  static bool TryGetPage(const std::string &category, int32_t offset,
      int32_t count, AppsResponse &result);
  static void PurgeAll();

private:
  static uint32_t CategoryCRC(const std::string &category);
  static std::string CachePath(const std::string &category);

  static bool OpenForWrite(const std::string &path, uint32_t &handle);
  static bool AppendItems(uint32_t handle, const std::vector<AppItem> &items);
  static bool FinaliseWrite(uint32_t handle, int32_t totalItems);
  static bool LoadFromDisk(const std::string &category);

  static DWORD WINAPI FetchThreadProc(LPVOID param);

  // Protected by mCacheLock -- read/written by both main and fetch threads
  static std::string mCachedCategory;
  static std::vector<AppItem> mCachedItems;
  static bool mCacheReady;
  static CRITICAL_SECTION mCacheLock;

  static volatile LONG mFetchCancel;
  static HANDLE mFetchThread;
  static volatile LONG mFetchFailed;
  static volatile LONG mStreamedCount;
  static volatile LONG mTotalCount;
};