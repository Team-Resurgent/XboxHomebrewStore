#include "AppSettings.h"
#include "Debug.h"
#include "FileSystem.h"

#define SETTINGS_PATH "T:\\Settings.bin"
#define DEFAULT_DOWNLOAD_PATH "HDD0-E:\\XHS Downloads\\"

AppSettingsData AppSettings::mData;
bool AppSettings::mLoaded = false;
bool AppSettings::mCacheLocationChanged = false;

// ==========================================================================
void AppSettings::ApplyDefaults() {
  memset(&mData, 0, sizeof(AppSettingsData));
  strncpy(mData.downloadPath, DEFAULT_DOWNLOAD_PATH, sizeof(mData.downloadPath) - 1);
  mData.afterInstallAction = (uint32_t)AfterInstallAsk;
  mData.showCachePartitions = 0;
  mData.preCacheOnIdle = 0;
  mData.retryFailedOnView = 0;
  mData.cacheLocation = (uint32_t)CacheLocationTemp;
  mData.cachePath[0] = '\0';
}

// ==========================================================================
bool AppSettings::Load() {
  ApplyDefaults();

  uint32_t fileHandle = 0;
  if (!FileSystem::FileOpen(SETTINGS_PATH, FileModeRead, fileHandle)) {
    Debug::Print("AppSettings: no settings file, using defaults.\n");
    mLoaded = true;
    return false;
  }

  uint32_t fileSize = 0;
  FileSystem::FileSize(fileHandle, fileSize);

  // Only read up to sizeof(AppSettingsData) ? handles older smaller files
  uint32_t readSize = fileSize < sizeof(AppSettingsData) ? fileSize : sizeof(AppSettingsData);
  uint32_t bytesRead = 0;
  bool ok = FileSystem::FileRead(fileHandle, (char *)&mData, readSize, bytesRead);
  FileSystem::FileClose(fileHandle);

  if (!ok || bytesRead == 0) {
    ApplyDefaults();
    mLoaded = true;
    return false;
  }

  // Null-terminate string fields defensively
  mData.downloadPath[sizeof(mData.downloadPath) - 1] = '\0';
  mData.cachePath[sizeof(mData.cachePath) - 1] = '\0';

  // If path was blank (older file), apply default
  if (mData.downloadPath[0] == '\0') {
    strncpy(mData.downloadPath, DEFAULT_DOWNLOAD_PATH, sizeof(mData.downloadPath) - 1);
  }

  // Clamp enum values
  if (mData.afterInstallAction > (uint32_t)AfterInstallAsk) {
    mData.afterInstallAction = (uint32_t)AfterInstallDelete;
  }
  if (mData.cacheLocation > (uint32_t)CacheLocationCustom) {
    mData.cacheLocation = (uint32_t)CacheLocationTemp;
  }

  mLoaded = true;
  Debug::Print("AppSettings: loaded.\n");
  return true;
}

// ==========================================================================
bool AppSettings::Save() {
  uint32_t bytesWritten = 0;
  bool ok = FileSystem::FileWrite(SETTINGS_PATH, (char *)&mData, sizeof(AppSettingsData), bytesWritten);
  if (!ok || bytesWritten != sizeof(AppSettingsData)) {
    Debug::Print("AppSettings: save failed.\n");
    return false;
  }
  Debug::Print("AppSettings: saved.\n");
  return true;
}

// ==========================================================================
// Accessors
// ==========================================================================
std::string AppSettings::GetDownloadPath() {
  if (!mLoaded) {
    Load();
  }
  return std::string(mData.downloadPath);
}

AfterInstallAction AppSettings::GetAfterInstallAction() {
  if (!mLoaded) {
    Load();
  }
  return (AfterInstallAction)mData.afterInstallAction;
}

bool AppSettings::GetShowCachePartitions() {
  if (!mLoaded) {
    Load();
  }
  return mData.showCachePartitions != 0;
}

bool AppSettings::GetPreCacheOnIdle() {
  if (!mLoaded) {
    Load();
  }
  return mData.preCacheOnIdle != 0;
}
bool AppSettings::GetRetryFailedOnView() {
  if (!mLoaded) { Load(); }
  return mData.retryFailedOnView != 0;
}

// ==========================================================================
// Mutators
// ==========================================================================
void AppSettings::SetDownloadPath(const std::string &path) {
  if (!mLoaded) {
    Load();
  }
  strncpy(mData.downloadPath, path.c_str(), sizeof(mData.downloadPath) - 1);
  mData.downloadPath[sizeof(mData.downloadPath) - 1] = '\0';
}

void AppSettings::SetAfterInstallAction(AfterInstallAction action) {
  if (!mLoaded) {
    Load();
  }
  mData.afterInstallAction = (uint32_t)action;
}

void AppSettings::SetShowCachePartitions(bool show) {
  if (!mLoaded) {
    Load();
  }
  mData.showCachePartitions = show ? 1 : 0;
}

void AppSettings::SetPreCacheOnIdle(bool enabled) {
  if (!mLoaded) {
    Load();
  }
  mData.preCacheOnIdle = enabled ? 1 : 0;
}
void AppSettings::SetRetryFailedOnView(bool enabled) {
  if (!mLoaded) { Load(); }
  mData.retryFailedOnView = enabled ? 1 : 0;
}

CacheLocation AppSettings::GetCacheLocation() {
  if (!mLoaded) {
    Load();
  }
  return (CacheLocation)mData.cacheLocation;
}

std::string AppSettings::GetCachePath() {
  if (!mLoaded) {
    Load();
  }
  return std::string(mData.cachePath);
}

void AppSettings::SetCacheLocation(CacheLocation location) {
  if (!mLoaded) {
    Load();
  }
  if (mData.cacheLocation != (uint32_t)location) {
    mData.cacheLocation = (uint32_t)location;
    mCacheLocationChanged = true;
  }
}

void AppSettings::SetCachePath(const std::string &path) {
  if (!mLoaded) {
    Load();
  }
  if (std::string(mData.cachePath) != path) {
    strncpy(mData.cachePath, path.c_str(), sizeof(mData.cachePath) - 1);
    mData.cachePath[sizeof(mData.cachePath) - 1] = '\0';
    mCacheLocationChanged = true;
  }
}

bool AppSettings::WasCacheLocationChanged() {
  return mCacheLocationChanged;
}

void AppSettings::ClearCacheLocationChangedFlag() {
  mCacheLocationChanged = false;
}