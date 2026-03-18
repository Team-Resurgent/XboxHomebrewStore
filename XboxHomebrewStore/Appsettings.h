#pragma once

#include "Main.h"

// What to do with the downloaded zip after install completes
typedef enum AfterInstallAction {
  AfterInstallDelete = 0,
  AfterInstallKeep = 1,
  AfterInstallAsk = 2,
} AfterInstallAction;

// Where to store covers, screenshots and metadata cache
typedef enum CacheLocation {
  CacheLocationTemp   = 0, // T:\Cache  (default -- fast, may be RAM disk)
  CacheLocationApp    = 1, // D:\Cache  (app drive -- always persistent)
  CacheLocationCustom = 2, // user-defined path
} CacheLocation;

// Flat binary struct persisted to T:\Settings.bin
// Add new fields at the END only to stay backwards-compatible
typedef struct {
  char downloadPath[256];
  uint32_t afterInstallAction;
  uint32_t showCachePartitions; // 0 = hidden (default), 1 = show X/Y/Z cache partitions
  uint32_t preCacheOnIdle;      // 0 = off (default), 1 = on -- idle cover pre-caching
  uint32_t cacheLocation;       // CacheLocation enum -- 0=T:, 1=D:, 2=Custom
  char cachePath[256];          // only used when cacheLocation == CacheLocationCustom
  uint32_t retryFailedOnView;   // 0 = off (default), 1 = retry failed cover/screenshot when viewing app
} AppSettingsData;

class AppSettings {
public:
  static bool Load();
  static bool Save();

  // Accessors
  static std::string GetDownloadPath();
  static AfterInstallAction GetAfterInstallAction();
  static bool GetShowCachePartitions();
  static bool GetPreCacheOnIdle();
  static bool GetRetryFailedOnView();
  static CacheLocation GetCacheLocation();
  static std::string GetCachePath();

  // Returns true if cache location was changed since last call to ClearCacheLocationChangedFlag()
  static bool WasCacheLocationChanged();
  static void ClearCacheLocationChangedFlag();

  // Mutators (call Save() to persist)
  static void SetDownloadPath(const std::string &path);
  static void SetAfterInstallAction(AfterInstallAction action);
  static void SetShowCachePartitions(bool show);
  static void SetPreCacheOnIdle(bool enabled);
  static void SetRetryFailedOnView(bool enabled);
  static void SetCacheLocation(CacheLocation location);
  static void SetCachePath(const std::string &path);

private:
  static void ApplyDefaults();
  static AppSettingsData mData;
  static bool mLoaded;
  static bool mCacheLocationChanged;
};