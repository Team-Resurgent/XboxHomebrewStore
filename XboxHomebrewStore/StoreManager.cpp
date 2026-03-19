#include "StoreManager.h"
#include "Context.h"
#include "Debug.h"
#include "Defines.h"
#include "ImageDownloader.h"
#include "Math.h"
#include "MetaCache.h"
#include "Models.h"
#include "TextureCache.h"
#include "TextureHelper.h"
#include "UserState.h"
#include "ViewState.h"
#include "WebManager.h"

namespace {
int32_t mCategoryIndex;
std::vector<StoreCategory> mCategories;
int32_t mWindowStoreItemOffset;
int32_t mWindowStoreItemCount;
StoreItem *mWindowStoreItems;
StoreItem *mTempStoreItems;

volatile LONG mIdleWarmerCancel;
HANDLE mIdleWarmerThread;
bool mIdleWarmerDone;
bool mWarmerQueued;  // set by thread when queuing completes; cleared on cancel/reset
ImageDownloader *mIdleWarmerDownloader;
} // namespace

static void CopyStoreItem(StoreItem &dst, const StoreItem &src) {
  dst.appId = src.appId;
  dst.name = src.name;
  dst.nameScrollState = src.nameScrollState;
  dst.author = src.author;
  dst.authorScrollState = src.authorScrollState;
  dst.category = src.category;
  dst.description = src.description;
  dst.latestVersion = src.latestVersion;
  dst.state = src.state;
  dst.cover = src.cover;
}

// ==========================================================================
// Init -- load categories, start MetaCache fetch, return immediately.
// StoreScene waits for MetaCache::IsReady() before calling RefreshApplications.
// ==========================================================================

bool StoreManager::Init() {
  mCategoryIndex = 0;
  mWindowStoreItemOffset = 0;
  mWindowStoreItemCount = 0;

  mWindowStoreItems = new StoreItem[Context::GetGridCells()];
  mTempStoreItems = new StoreItem[Context::GetGridCols()];

  mIdleWarmerCancel = 0;
  mIdleWarmerThread = NULL;
  mIdleWarmerDone = false;
  mWarmerQueued = false;
  mIdleWarmerDownloader = new ImageDownloader();

  // Init MetaCache FIRST so its critical section is always initialised
  // even if LoadCategories fails (bad URL at startup)
  MetaCache::Init();

  if (!LoadCategories()) {
    MetaCache::SetFailed();
    return false;
  }

  std::string categoryFilter = "";
  int32_t total = GetSelectedCategoryTotal();
  MetaCache::StartFetch(categoryFilter, total);

  return true;
}

// ==========================================================================
// Idle Warmer -- waits for MetaCache then downloads missing covers
// ==========================================================================

void StoreManager::StopIdleWarmer() {
  InterlockedExchange((LPLONG)&mIdleWarmerCancel, 1);
  mWarmerQueued = false;
  if (mIdleWarmerDownloader != NULL) {
    mIdleWarmerDownloader->CancelAll();
  }
  if (mIdleWarmerThread == NULL) {
    InterlockedExchange((LPLONG)&mIdleWarmerCancel, 0);
    return;
  }
  mIdleWarmerDone = false;
  mWarmerQueued = false;
}

bool StoreManager::IsIdleWarmerRunning() { return mIdleWarmerThread != NULL; }
bool StoreManager::IsIdleWarmerDone() { return mIdleWarmerDone; }
void StoreManager::SetIdleWarmerDone() { mIdleWarmerDone = true; }
bool StoreManager::IsWarmerQueued() { return mWarmerQueued; }
bool StoreManager::IsIdleWarmerDownloading() {
  return mIdleWarmerDownloader != NULL && mIdleWarmerDownloader->HasPendingWork();
}

void StoreManager::StartIdleWarmer() {
  if (mIdleWarmerThread != NULL) {
    return;
  }
  if (mIdleWarmerDone) {
    return;
  }

  int32_t total = GetSelectedCategoryTotal();
  int32_t cached = ImageDownloader::GetCachedCoverCount();
  if (cached >= total) {
    Debug::Print("IdleWarmer: all %d covers already cached, skipping\n", total);
    mIdleWarmerDone = true;
    return;
  }

  InterlockedExchange((LPLONG)&mIdleWarmerCancel, 0);

  HANDLE h = CreateThread(NULL, 0, IdleWarmerThreadProc, NULL, 0, NULL);
  mIdleWarmerThread = h;
  if (h != NULL) {
    CloseHandle(h);
  } else {
    Debug::Print("IdleWarmer: CreateThread failed\n");
  }
}

DWORD WINAPI StoreManager::IdleWarmerThreadProc(LPVOID param) {
  (void)param;
  Debug::Print("IdleWarmer: started, waiting for MetaCache\n");

  // Wait for MetaCache to be ready -- no API calls, read from memory
  std::vector<std::string> appIds;
  std::string categoryFilter = "";

  while (mIdleWarmerCancel == 0) {
    if (MetaCache::IsReady()) {
      int32_t total = StoreManager::GetSelectedCategoryTotal();
      for (int32_t offset = 0; offset < total && mIdleWarmerCancel == 0; offset += 100) {
        AppsResponse page;
        if (MetaCache::TryGetPage(categoryFilter, offset, 100, page)) {
          for (int32_t i = 0; i < (int32_t)page.items.size(); i++) {
            appIds.push_back(page.items[i].id);
          }
        }
      }
      break;
    }
    Sleep(100);
  }

  if (mIdleWarmerCancel != 0) {
    Debug::Print("IdleWarmer: cancelled during collect\n");
    InterlockedExchange((LPLONG)&mIdleWarmerCancel, 0);
    mIdleWarmerThread = NULL;
    return 0;
  }

  Debug::Print("IdleWarmer: collected %d appIds from MetaCache, queuing covers\n", (int32_t)appIds.size());

  // Queue only missing covers
  int32_t queued = 0;
  for (int32_t i = 0; i < (int32_t)appIds.size(); i++) {
    if (mIdleWarmerCancel != 0) {
      break;
    }
    if (mIdleWarmerDownloader != NULL) {
      if (!ImageDownloader::IsCoverCached(appIds[i]) &&
          !ImageDownloader::IsCoverFailed(appIds[i])) {
        mIdleWarmerDownloader->WarmCache(appIds[i], IMAGE_COVER);
        queued++;
      }
    }
  }

  if (mIdleWarmerCancel == 0) {
    Debug::Print("IdleWarmer: queued %d covers for download\n", queued);
    mWarmerQueued = true;  // signal Update that downloads are in flight
  } else {
    Debug::Print("IdleWarmer: cancelled during queue\n");
    InterlockedExchange((LPLONG)&mIdleWarmerCancel, 0);
  }

  mIdleWarmerThread = NULL;
  return 0;
}

// ==========================================================================
// Reset
// ==========================================================================

bool StoreManager::Reset() {
  StopIdleWarmer();
  mIdleWarmerDone = false;
  mWarmerQueued = false;
  MetaCache::StopFetch();

  TextureCache::Clear();

  for (int32_t i = 0; i < mWindowStoreItemCount; i++) {
    mWindowStoreItems[i].cover = NULL; // view pointer -- TextureCache owns lifetime
  }
  for (int32_t i = 0; i < Context::GetGridCells(); i++) {
    mWindowStoreItems[i].appId = "";
    mWindowStoreItems[i].name = "";
    mWindowStoreItems[i].author = "";
    mWindowStoreItems[i].category = "";
    mWindowStoreItems[i].description = "";
    mWindowStoreItems[i].latestVersion = "";
    mWindowStoreItems[i].state = 0;
    mWindowStoreItems[i].cover = NULL;
  }
  for (int32_t i = 0; i < Context::GetGridCols(); i++) {
    mTempStoreItems[i].appId = "";
    mTempStoreItems[i].name = "";
    mTempStoreItems[i].author = "";
    mTempStoreItems[i].category = "";
    mTempStoreItems[i].description = "";
    mTempStoreItems[i].latestVersion = "";
    mTempStoreItems[i].state = 0;
    mTempStoreItems[i].cover = NULL;
  }
  mCategoryIndex = 0;
  mWindowStoreItemOffset = 0;
  mWindowStoreItemCount = 0;
  mCategories.clear();

  MetaCache::PurgeAll();

  if (!LoadCategories()) {
    Debug::Print("StoreManager::Reset: LoadCategories failed\n");
    MetaCache::SetFailed();
    return false;
  }

  std::string categoryFilter = "";
  int32_t total = GetSelectedCategoryTotal();
  MetaCache::StartFetch(categoryFilter, total);

  return true;
}

// ==========================================================================
// Accessors
// ==========================================================================

int32_t StoreManager::GetCategoryCount() { return (int32_t)mCategories.size(); }

int32_t StoreManager::GetCategoryIndex() { return mCategoryIndex; }

void StoreManager::SetCategoryIndex(int32_t categoryIndex) {
  StopIdleWarmer();
  mIdleWarmerDone = false;
  mWarmerQueued = false;
  MetaCache::StopFetch();

  // Release covers from current window
  for (int32_t i = 0; i < mWindowStoreItemCount; i++) {
    mWindowStoreItems[i].cover = NULL; // view pointer -- TextureCache owns lifetime
  }
  mWindowStoreItemOffset = 0;
  mWindowStoreItemCount = 0;

  // Purge all cached metadata
  MetaCache::PurgeAll();

  // Re-fetch categories from server so counts are fresh (a new item may have
  // been published since startup). This is the same thing Reset() does.
  // If it fails we keep the old mCategories so the UI stays usable.
  if (!LoadCategories()) {
    Debug::Print("SetCategoryIndex: LoadCategories failed, using stale counts\n");
  }

  // Clamp index to new category list size in case it changed
  if (categoryIndex < 0 || categoryIndex >= (int32_t)mCategories.size()) {
    categoryIndex = 0;
  }
  mCategoryIndex = categoryIndex;

  std::string categoryFilter = "";
  if (mCategoryIndex > 0 && mCategoryIndex < (int32_t)mCategories.size()) {
    categoryFilter = mCategories[mCategoryIndex].name;
  }
  int32_t total = GetSelectedCategoryTotal();
  MetaCache::StartFetch(categoryFilter, total);

  // StoreScene will see !MetaCache::IsReady() and show loading until done
}

StoreCategory *StoreManager::GetStoreCategory(int32_t categoryIndex) {
  if (categoryIndex < 0 || categoryIndex >= (int32_t)mCategories.size()) {
    return NULL;
  }
  return &mCategories[categoryIndex];
}

int32_t StoreManager::GetSelectedCategoryTotal() {
  if (mCategories.empty()) {
    return 0;
  }
  return mCategories[mCategoryIndex].count;
}

std::string StoreManager::GetSelectedCategoryName() {
  if (mCategories.empty() || mCategoryIndex <= 0 ||
      mCategoryIndex >= (int32_t)mCategories.size()) {
    return "";
  }
  return mCategories[mCategoryIndex].name;
}

int32_t StoreManager::GetWindowStoreItemOffset() {
  return mWindowStoreItemOffset;
}

int32_t StoreManager::GetWindowStoreItemCount() {
  return mWindowStoreItemCount;
}

StoreItem *StoreManager::GetWindowStoreItem(int32_t storeItemIndex) {
  if (storeItemIndex < 0 || storeItemIndex >= mWindowStoreItemCount) {
    return NULL;
  }
  return &mWindowStoreItems[storeItemIndex];
}

// InvalidateCovers -- call after deleting cover files on disk.
// Releases all GPU textures from the LRU cache and nulls cover pointers on
// all window items so DrawStoreItem re-evaluates them next frame.
void StoreManager::InvalidateCovers() {
  // Cancel any in-flight warmer and reset done state so the 30s countdown
  // starts fresh -- covers need re-downloading after cache clear.
  StopIdleWarmer();
  mIdleWarmerDone = false;
  mWarmerQueued = false;
  TextureCache::Clear();
  int32_t i;
  for (i = 0; i < mWindowStoreItemCount; i++) {
    mWindowStoreItems[i].cover = NULL;
  }
}

bool StoreManager::HasPrevious() {
  return mWindowStoreItemOffset > 0;
}

bool StoreManager::HasNext() {
  if (mCategories.empty()) {
    return false;
  }
  int32_t categoryItemCount = mCategories[mCategoryIndex].count;
  return (mWindowStoreItemOffset + mWindowStoreItemCount) < categoryItemCount;
}

// ==========================================================================
// LoadPrevious -- scroll up one row, read from MetaCache
// ==========================================================================

bool StoreManager::LoadPrevious() {
  if (!HasPrevious()) {
    return false;
  }

  int32_t newOffset = mWindowStoreItemOffset - Context::GetGridCols();
  int32_t loadedCount = 0;

  if (!LoadPage(mTempStoreItems, newOffset, Context::GetGridCols(), &loadedCount)) {
    return false;
  }

  int32_t remainder = mWindowStoreItemCount % Context::GetGridCols();
  int32_t itemsToRemove = remainder ? remainder : Context::GetGridCols();

  for (int32_t i = 0; i < itemsToRemove; i++) {
    StoreItem &storeItem = mWindowStoreItems[mWindowStoreItemCount - 1 - i];
    if (storeItem.cover != NULL) {
      storeItem.cover = NULL; // view pointer -- TextureCache owns lifetime
    }
  }

  for (int32_t i = mWindowStoreItemCount - itemsToRemove; i > 0; i--) {
    CopyStoreItem(mWindowStoreItems[i - 1 + loadedCount], mWindowStoreItems[i - 1]);
  }

  for (int32_t i = 0; i < loadedCount; i++) {
    CopyStoreItem(mWindowStoreItems[i], mTempStoreItems[i]);
  }

  mWindowStoreItemOffset = newOffset;
  mWindowStoreItemCount = mWindowStoreItemCount - itemsToRemove + loadedCount;

  return true;
}

// ==========================================================================
// LoadNext -- scroll down one row, read from MetaCache
// ==========================================================================

bool StoreManager::LoadNext() {
  if (!HasNext()) {
    return false;
  }

  int32_t newOffset = mWindowStoreItemOffset + Context::GetGridCols();
  int32_t tempOffset = Context::GetGridCols() * Math::MaxInt32(0, Context::GetGridRows() - 1);
  int32_t fetchOffset = newOffset + tempOffset;

  int32_t loadedCount = 0;
  if (!LoadPage(mTempStoreItems, fetchOffset, Context::GetGridCols(), &loadedCount)) {
    return false;
  }

  int32_t itemsToRemove = Context::GetGridCols();
  for (int32_t i = 0; i < itemsToRemove; i++) {
    StoreItem &storeItem = mWindowStoreItems[i];
    if (storeItem.cover != NULL) {
      storeItem.cover = NULL; // view pointer -- TextureCache owns lifetime
    }
  }

  for (int32_t i = itemsToRemove; i < mWindowStoreItemCount; i++) {
    CopyStoreItem(mWindowStoreItems[i - itemsToRemove], mWindowStoreItems[i]);
  }

  int32_t remainingCount = mWindowStoreItemCount - itemsToRemove;
  for (int32_t i = 0; i < loadedCount; i++) {
    int32_t dest = remainingCount + i;
    if (dest < Context::GetGridCells()) {
      CopyStoreItem(mWindowStoreItems[dest], mTempStoreItems[i]);
    }
  }

  mWindowStoreItemOffset = newOffset;
  mWindowStoreItemCount = Math::MinInt32(remainingCount + loadedCount, Context::GetGridCells());

  return true;
}

// ==========================================================================
// LoadAtOffset -- jump to any position
// ==========================================================================

bool StoreManager::LoadAtOffset(int32_t offset) {
  for (int32_t i = 0; i < mWindowStoreItemCount; i++) {
    mWindowStoreItems[i].cover = NULL; // view pointer -- TextureCache owns lifetime
  }
  for (int32_t i = 0; i < Context::GetGridCells(); i++) {
    mWindowStoreItems[i].appId = "";
    mWindowStoreItems[i].name = "";
    mWindowStoreItems[i].author = "";
    mWindowStoreItems[i].category = "";
    mWindowStoreItems[i].description = "";
    mWindowStoreItems[i].latestVersion = "";
    mWindowStoreItems[i].state = 0;
    mWindowStoreItems[i].cover = NULL;
  }
  mWindowStoreItemCount = 0;
  mWindowStoreItemOffset = 0;

  int32_t loadedCount = 0;
  if (!LoadPage(mWindowStoreItems, offset, Context::GetGridCells(), &loadedCount)) {
    return false;
  }

  mWindowStoreItemOffset = offset;
  mWindowStoreItemCount = loadedCount;
  return true;
}

// ==========================================================================
// TryGetStoreVersions
// ==========================================================================

bool StoreManager::TryGetStoreVersions(std::string appId, StoreVersions *storeVersions) {
  StoreItem *storeItem = NULL;
  int32_t count = GetWindowStoreItemCount();
  for (int32_t i = 0; i < count; i++) {
    StoreItem *item = GetWindowStoreItem(i);
    if (item != NULL && item->appId == appId) {
      storeItem = item;
      break;
    }
  }

  if (storeItem == NULL) {
    return false;
  }

  VersionsResponse versionsResponse;
  if (!WebManager::TryGetVersions(appId, versionsResponse)) {
    return false;
  }

  if (versionsResponse.versions.empty()) {
    Debug::Print("TryGetStoreVersions: no versions for %s\n", appId.c_str());
    return false;
  }

  storeVersions->appId = storeItem->appId;
  storeVersions->name = versionsResponse.name;
  storeVersions->author = versionsResponse.author;
  storeVersions->description = versionsResponse.description;
  storeVersions->latestVersion = versionsResponse.latestVersion;
  storeVersions->cover = NULL;
  storeVersions->screenshot = NULL;
  storeVersions->versions.clear();

  // Find the index of the installed version in the list (API returns newest first).
  // Only versions with a lower index (i.e. newer) should be marked as updates.
  int32_t installedVersionIndex = -1;
  {
    std::vector<UserSaveState> userStates;
    if (UserState::TryGetByAppId(storeItem->appId, userStates)) {
      for (int32_t i = 0; i < (int32_t)versionsResponse.versions.size(); i++) {
        for (size_t j = 0; j < userStates.size(); j++) {
              if (userStates[j].versionId[0] != '\0' &&
              versionsResponse.versions[i].id == userStates[j].versionId) {
            installedVersionIndex = i;
                  break;
          }
        }
        if (installedVersionIndex >= 0) break;
      }
    } else {
    }
  }

  for (int32_t i = 0; i < (int32_t)versionsResponse.versions.size(); i++) {
    VersionItem *versionItem = &versionsResponse.versions[i];

    StoreVersion storeVersion;
    storeVersion.appId = storeItem->appId;
    storeVersion.versionId = versionItem->id;
    storeVersion.version = versionItem->version;
    storeVersion.versionScrollState.active = false;
    storeVersion.size = versionItem->size;
    storeVersion.releaseDate = versionItem->releaseDate;
    storeVersion.changeLog = versionItem->changeLog;
    storeVersion.titleId = versionItem->titleId;
    storeVersion.region = versionItem->region;
    storeVersion.downloadFiles = versionItem->downloadFiles;
    storeVersion.folderName = versionItem->folderName;
    storeVersion.state = (i == 0) ? storeItem->state : 0;

    if (ViewState::GetViewed(storeItem->appId, storeVersions->latestVersion)) {
      storeVersion.state = 0;
    }

    std::vector<UserSaveState> userStates;
    if (UserState::TryGetByAppId(storeItem->appId, userStates)) {
      bool hasInstalled = false;
      bool hasThisVersion = (installedVersionIndex == i);
      for (size_t j = 0; j < userStates.size(); j++) {
        const UserSaveState &us = userStates[j];
        if (us.installPath[0] != '\0') {
          hasInstalled = true;
          storeVersion.state = 0;
        }
      }
      // Only the latest version (index 0) can ever be an update.
      // If it's not installed and something older is, mark it as Update.
      if (hasInstalled && !hasThisVersion &&
          i == 0 && installedVersionIndex > 0) {
        storeVersion.state = 2;
      }
    }

    storeVersions->versions.push_back(storeVersion);
  }

  return true;
}

// ==========================================================================
// Private
// ==========================================================================

bool StoreManager::LoadCategories() {
  mCategories.clear();

  CategoriesResponse categoriesResponse;
  if (!WebManager::TryGetCategories(categoriesResponse)) {
    return false;
  }

  StoreCategory allApps;
  allApps.name = "All Apps";
  allApps.count = 0;
  allApps.nameScrollState.active = false;

  for (int32_t i = 0; i < (int32_t)categoriesResponse.size(); i++) {
    StoreCategory storeCategory;
    storeCategory.name = categoriesResponse[i].name;
    storeCategory.nameScrollState.active = false;
    storeCategory.count = categoriesResponse[i].count;
    mCategories.push_back(storeCategory);
    allApps.count += categoriesResponse[i].count;
  }

  mCategories.insert(mCategories.begin(), allApps);
  return true;
}

// LoadPage -- MetaCache only, no API fallback. Only called when IsReady().
bool StoreManager::LoadPage(StoreItem *dest, int32_t offset, int32_t count, int32_t *loadedCount) {
  std::string categoryFilter = "";
  if (mCategoryIndex > 0 && mCategoryIndex < (int32_t)mCategories.size()) {
    categoryFilter = mCategories[mCategoryIndex].name;
  }

  AppsResponse response;
  if (!MetaCache::TryGetPage(categoryFilter, offset, count, response)) {
    Debug::Print("LoadPage: MetaCache not ready at offset=%d\n", offset);
    return false;
  }

  const int32_t itemCount = (int32_t)response.items.size();
  *loadedCount = itemCount;

  for (int32_t i = 0; i < itemCount; i++) {
    AppItem *appItem = &response.items[i];
    dest[i].appId = appItem->id;
    dest[i].name = appItem->name;
    dest[i].nameScrollState.active = false;
    dest[i].author = appItem->author;
    dest[i].authorScrollState.active = false;
    dest[i].category = appItem->category;
    dest[i].description = appItem->description;
    dest[i].latestVersion = appItem->latestVersion;
    dest[i].state = ViewState::GetViewed(appItem->id, dest[i].latestVersion)
        ? 0
        : appItem->state;
    dest[i].cover = NULL;

    std::vector<UserSaveState> userStates;
    if (UserState::TryGetByAppId(dest[i].appId, userStates)) {
      bool hasInstalled = false;
      bool hasLatestVersion = false;
      for (size_t j = 0; j < userStates.size(); j++) {
        const UserSaveState &us = userStates[j];
        if (us.installPath[0] != '\0') {
          hasInstalled = true;
          dest[i].state = 0;
        }
        if (us.versionId[0] != '\0' && dest[i].latestVersion == us.versionId) {
          hasLatestVersion = true;
        }
      }
      if (hasInstalled && !hasLatestVersion) {
        dest[i].state = 2;
      }
    }
  }

  return true;
}

bool StoreManager::RefreshApplications() {
  for (int32_t i = 0; i < mWindowStoreItemCount; i++) {
    StoreItem &storeItem = mWindowStoreItems[i];
    if (storeItem.cover != NULL) {
      storeItem.cover = NULL; // view pointer -- TextureCache owns lifetime
    }
  }

  mWindowStoreItemOffset = 0;
  mWindowStoreItemCount = 0;

  int32_t loadedCount = 0;
  if (!LoadPage(mWindowStoreItems, 0, Context::GetGridCells(), &loadedCount)) {
    return false;
  }

  mWindowStoreItemCount = loadedCount;
  return true;
}