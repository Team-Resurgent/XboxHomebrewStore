#pragma once

#include "Main.h"
#include "Models.h"

class MetaCache {
public:
  static void Init();
  static void StartFetch(const std::string &category, int32_t totalCount);
  static void StopFetch();
  static bool IsFetching();
  static bool IsReady();
  static int32_t GetItemCount(); // actual count from cache, may differ from category metadata
  static bool TryGetPage(const std::string &category, int32_t offset,
      int32_t count, AppsResponse &result);
  static void PurgeCategory(const std::string &category);
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
};