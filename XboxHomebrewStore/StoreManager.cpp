#include "StoreManager.h"
#include "WebManager.h"
#include "Models.h"
#include "Math.h"
#include "Defines.h"
#include "Context.h"
#include "TextureHelper.h"
#include "ImageDownloader.h"
#include "UserState.h"
#include "ViewState.h"
#include "Debug.h"

#define PREFETCH_IDLE    0
#define PREFETCH_RUNNING 1
#define PREFETCH_READY   2
#define PREFETCH_FAILED  3

namespace {
    int32_t mCategoryIndex;
    std::vector<StoreCategory> mCategories;
    int32_t mWindowStoreItemOffset;
    int32_t mWindowStoreItemCount;
    StoreItem* mWindowStoreItems;
    StoreItem* mTempStoreItems;

    // Next prefetch -- one row lookahead forward
    StoreItem*    mPrefetchItems;
    int32_t       mPrefetchOffset;
    int32_t       mPrefetchCount;
    int32_t       mPrefetchCategory;
    volatile LONG mPrefetchState;
    volatile LONG mPrefetchCancel;
    HANDLE        mPrefetchThread;

    // Prev prefetch -- one row lookahead backward
    StoreItem*    mPrefetchPrevItems;
    int32_t       mPrefetchPrevOffset;
    int32_t       mPrefetchPrevCount;
    int32_t       mPrefetchPrevCategory;
    volatile LONG mPrefetchPrevState;
    volatile LONG mPrefetchPrevCancel;
    HANDLE        mPrefetchPrevThread;

    // Cover warmer -- background cover cache downloads for prefetched rows
    ImageDownloader* mCoverWarmer;

    // Idle warmer -- walks all apps page by page when user is idle
    volatile LONG    mIdleWarmerCancel;
    HANDLE           mIdleWarmerThread;
    bool             mIdleWarmerDone;       // true once fully finished -- don't restart
    ImageDownloader* mIdleWarmerDownloader; // dedicated downloader, not shared with prefetch
}

static void CopyStoreItem(StoreItem& dst, const StoreItem& src)
{
    dst.appId                    = src.appId;
    dst.name                     = src.name;
    dst.nameScrollState          = src.nameScrollState;
    dst.author                   = src.author;
    dst.authorScrollState        = src.authorScrollState;
    dst.category                 = src.category;
    dst.description              = src.description;
    dst.latestVersion            = src.latestVersion;
    dst.state                    = src.state;
    dst.cover                    = src.cover;
}

// ==========================================================================
// Init
// ==========================================================================

bool StoreManager::Init()
{
    mCategoryIndex         = 0;
    mWindowStoreItemOffset = 0;
    mWindowStoreItemCount  = 0;

    mWindowStoreItems  = new StoreItem[Context::GetGridCells()];
    mTempStoreItems    = new StoreItem[Context::GetGridCols()];

    mPrefetchItems     = new StoreItem[Context::GetGridCols()];
    mPrefetchOffset    = -1;
    mPrefetchCount     = 0;
    mPrefetchCategory  = -1;
    mPrefetchState     = PREFETCH_IDLE;
    mPrefetchCancel    = 0;
    mPrefetchThread    = NULL;

    mPrefetchPrevItems    = new StoreItem[Context::GetGridCols()];
    mPrefetchPrevOffset   = -1;
    mPrefetchPrevCount    = 0;
    mPrefetchPrevCategory = -1;
    mPrefetchPrevState    = PREFETCH_IDLE;
    mPrefetchPrevCancel   = 0;
    mPrefetchPrevThread   = NULL;

    mCoverWarmer = new ImageDownloader();

    mIdleWarmerCancel     = 0;
    mIdleWarmerThread     = NULL;
    mIdleWarmerDone       = false;
    mIdleWarmerDownloader = new ImageDownloader();

    if (!LoadCategories())
        return false;

    return RefreshApplications();
}

// ==========================================================================
// Prefetch
// ==========================================================================

void StoreManager::CancelPrefetch()
{
    // Cancel next thread
    InterlockedExchange((LPLONG)&mPrefetchCancel, 1);
    if (mPrefetchThread != NULL)
    {
        WaitForSingleObject(mPrefetchThread, 3000);
        CloseHandle(mPrefetchThread);
        mPrefetchThread = NULL;
    }
    mPrefetchState  = PREFETCH_IDLE;
    mPrefetchOffset = -1;
    mPrefetchCount  = 0;
    InterlockedExchange((LPLONG)&mPrefetchCancel, 0);

    // Cancel prev thread
    InterlockedExchange((LPLONG)&mPrefetchPrevCancel, 1);
    if (mPrefetchPrevThread != NULL)
    {
        WaitForSingleObject(mPrefetchPrevThread, 3000);
        CloseHandle(mPrefetchPrevThread);
        mPrefetchPrevThread = NULL;
    }
    mPrefetchPrevState  = PREFETCH_IDLE;
    mPrefetchPrevOffset = -1;
    mPrefetchPrevCount  = 0;
    InterlockedExchange((LPLONG)&mPrefetchPrevCancel, 0);

    // Cancel any in-flight cover warming
    if (mCoverWarmer != NULL)
        mCoverWarmer->CancelAll();
}

// ==========================================================================
// Idle warmer -- walks all apps from offset 0, caches covers silently
// ==========================================================================

void StoreManager::StopIdleWarmer()
{
    InterlockedExchange((LPLONG)&mIdleWarmerCancel, 1);
    if (mIdleWarmerThread != NULL)
    {
        WaitForSingleObject(mIdleWarmerThread, 5000);
        CloseHandle(mIdleWarmerThread);
        mIdleWarmerThread = NULL;
    }
    InterlockedExchange((LPLONG)&mIdleWarmerCancel, 0);
    mIdleWarmerDone = false;  // allow restart next idle cycle
    if (mIdleWarmerDownloader != NULL)
        mIdleWarmerDownloader->CancelAll();
}

bool StoreManager::IsIdleWarmerRunning()
{
    return mIdleWarmerThread != NULL;
}

void StoreManager::StartIdleWarmer()
{
    // Don't start if already running or already finished this session
    if (mIdleWarmerThread != NULL)
        return;
    if (mIdleWarmerDone)
        return;

    // Check if everything is already cached -- no point starting at all
    int32_t cached = ImageDownloader::GetCachedCoverCount();
    int32_t total  = GetSelectedCategoryTotal();
    if (total > 0 && cached >= total)
    {
        Debug::Print("IdleWarmer: all %d covers already cached, skipping\n", total);
        mIdleWarmerDone = true;
        return;
    }

    InterlockedExchange((LPLONG)&mIdleWarmerCancel, 0);
    mIdleWarmerThread = CreateThread(NULL, 0, IdleWarmerThreadProc, NULL, 0, NULL);
}

DWORD WINAPI StoreManager::IdleWarmerThreadProc(LPVOID param)
{
    (void)param;
    Debug::Print("IdleWarmer: started\n");

    const int32_t pageSize = Context::GetGridCols();
    int32_t offset = 0;

    while (mIdleWarmerCancel == 0)
    {
        StoreItem* page = new StoreItem[pageSize];
        int32_t loadedCount = 0;

        bool ok = StoreManager::LoadApplications(page, offset, pageSize, &loadedCount);

        if (!ok || loadedCount == 0)
        {
            delete[] page;
            break;
        }

        for (int32_t i = 0; i < loadedCount; i++)
        {
            if (mIdleWarmerCancel != 0)
                break;
            if (mIdleWarmerDownloader != NULL)
                mIdleWarmerDownloader->WarmCache(page[i].appId, IMAGE_COVER);
        }

        delete[] page;

        if (mIdleWarmerCancel != 0)
            break;

        offset += loadedCount;

        if (loadedCount < pageSize)
        {
            // Reached end of catalogue cleanly
            Debug::Print("IdleWarmer: finished (%d covers queued)\n", offset);
            mIdleWarmerDone = true;
            mIdleWarmerThread = NULL;
            return 0;
        }
    }

    Debug::Print("IdleWarmer: cancelled (offset=%d)\n", offset);
    mIdleWarmerThread = NULL;
    return 0;
}

DWORD WINAPI StoreManager::PrefetchNextThreadProc(LPVOID param)
{
    int32_t* args          = (int32_t*)param;
    int32_t targetOffset   = args[0];
    int32_t targetCategory = args[1];
    free(param);

    if (mPrefetchCancel != 0)
    {
        InterlockedExchange((LPLONG)&mPrefetchState, PREFETCH_FAILED);
        return 0;
    }

    int32_t loadedCount = 0;
    bool ok = StoreManager::LoadApplications(
        mPrefetchItems, targetOffset, Context::GetGridCols(), &loadedCount);

    if (mPrefetchCancel != 0)
    {
        InterlockedExchange((LPLONG)&mPrefetchState, PREFETCH_FAILED);
        return 0;
    }

    if (!ok)
    {
        Debug::Print("PrefetchNext failed offset=%d\n", targetOffset);
        InterlockedExchange((LPLONG)&mPrefetchState, PREFETCH_FAILED);
        return 0;
    }

    mPrefetchCount    = loadedCount;
    mPrefetchOffset   = targetOffset;
    mPrefetchCategory = targetCategory;

    // Warm covers BEFORE signalling ready -- avoids race where main thread
    // consumes mPrefetchItems while we're still iterating them here
    for (int32_t i = 0; i < loadedCount; i++)
        mCoverWarmer->WarmCache(mPrefetchItems[i].appId, IMAGE_COVER);

    InterlockedExchange((LPLONG)&mPrefetchState, PREFETCH_READY);
    Debug::Print("PrefetchNext ready offset=%d count=%d\n", targetOffset, loadedCount);
    return 0;
}

DWORD WINAPI StoreManager::PrefetchPrevThreadProc(LPVOID param)
{
    int32_t* args          = (int32_t*)param;
    int32_t targetOffset   = args[0];
    int32_t targetCategory = args[1];
    free(param);

    if (mPrefetchPrevCancel != 0)
    {
        InterlockedExchange((LPLONG)&mPrefetchPrevState, PREFETCH_FAILED);
        return 0;
    }

    int32_t loadedCount = 0;
    bool ok = StoreManager::LoadApplications(
        mPrefetchPrevItems, targetOffset, Context::GetGridCols(), &loadedCount);

    if (mPrefetchPrevCancel != 0)
    {
        InterlockedExchange((LPLONG)&mPrefetchPrevState, PREFETCH_FAILED);
        return 0;
    }

    if (!ok)
    {
        Debug::Print("PrefetchPrev failed offset=%d\n", targetOffset);
        InterlockedExchange((LPLONG)&mPrefetchPrevState, PREFETCH_FAILED);
        return 0;
    }

    mPrefetchPrevCount    = loadedCount;
    mPrefetchPrevOffset   = targetOffset;
    mPrefetchPrevCategory = targetCategory;

    // Warm covers BEFORE signalling ready -- avoids race where main thread
    // consumes mPrefetchPrevItems while we're still iterating them here
    for (int32_t i = 0; i < loadedCount; i++)
        mCoverWarmer->WarmCache(mPrefetchPrevItems[i].appId, IMAGE_COVER);

    InterlockedExchange((LPLONG)&mPrefetchPrevState, PREFETCH_READY);
    Debug::Print("PrefetchPrev ready offset=%d count=%d\n", targetOffset, loadedCount);
    return 0;
}

void StoreManager::KickPrefetch()
{
    // -- Next direction --
    if (HasNext())
    {
        int32_t nextRowOffset = mWindowStoreItemOffset
            + (Context::GetGridCols() * Math::MaxInt32(0, Context::GetGridRows() - 1))
            + Context::GetGridCols();

        bool nextAlreadyCovered =
            (mPrefetchState == PREFETCH_READY || mPrefetchState == PREFETCH_RUNNING)
            && mPrefetchOffset   == nextRowOffset
            && mPrefetchCategory == mCategoryIndex;

        if (!nextAlreadyCovered)
        {
            InterlockedExchange((LPLONG)&mPrefetchCancel, 1);
            if (mPrefetchThread != NULL)
            {
                WaitForSingleObject(mPrefetchThread, 3000);
                CloseHandle(mPrefetchThread);
                mPrefetchThread = NULL;
            }
            mPrefetchState  = PREFETCH_IDLE;
            mPrefetchOffset = -1;
            mPrefetchCount  = 0;
            InterlockedExchange((LPLONG)&mPrefetchCancel, 0);

            int32_t* params = (int32_t*)malloc(sizeof(int32_t) * 2);
            params[0] = nextRowOffset;
            params[1] = mCategoryIndex;
            InterlockedExchange((LPLONG)&mPrefetchState, PREFETCH_RUNNING);
            mPrefetchThread = CreateThread(NULL, 0, PrefetchNextThreadProc, params, 0, NULL);
            if (mPrefetchThread == NULL)
            {
                free(params);
                InterlockedExchange((LPLONG)&mPrefetchState, PREFETCH_IDLE);
                Debug::Print("PrefetchNext: CreateThread failed\n");
            }
        }
    }

    // -- Prev direction --
    if (HasPrevious())
    {
        int32_t prevRowOffset = mWindowStoreItemOffset - Context::GetGridCols();

        bool prevAlreadyCovered =
            (mPrefetchPrevState == PREFETCH_READY || mPrefetchPrevState == PREFETCH_RUNNING)
            && mPrefetchPrevOffset   == prevRowOffset
            && mPrefetchPrevCategory == mCategoryIndex;

        if (!prevAlreadyCovered)
        {
            InterlockedExchange((LPLONG)&mPrefetchPrevCancel, 1);
            if (mPrefetchPrevThread != NULL)
            {
                WaitForSingleObject(mPrefetchPrevThread, 3000);
                CloseHandle(mPrefetchPrevThread);
                mPrefetchPrevThread = NULL;
            }
            mPrefetchPrevState  = PREFETCH_IDLE;
            mPrefetchPrevOffset = -1;
            mPrefetchPrevCount  = 0;
            InterlockedExchange((LPLONG)&mPrefetchPrevCancel, 0);

            int32_t* params = (int32_t*)malloc(sizeof(int32_t) * 2);
            params[0] = prevRowOffset;
            params[1] = mCategoryIndex;
            InterlockedExchange((LPLONG)&mPrefetchPrevState, PREFETCH_RUNNING);
            mPrefetchPrevThread = CreateThread(NULL, 0, PrefetchPrevThreadProc, params, 0, NULL);
            if (mPrefetchPrevThread == NULL)
            {
                free(params);
                InterlockedExchange((LPLONG)&mPrefetchPrevState, PREFETCH_IDLE);
                Debug::Print("PrefetchPrev: CreateThread failed\n");
            }
        }
    }
}


// ==========================================================================
// Reset
// ==========================================================================

bool StoreManager::Reset()
{
    StopIdleWarmer();
    mIdleWarmerDone = false;
    CancelPrefetch();

    for (int32_t i = 0; i < mWindowStoreItemCount; i++)
    {
        if (mWindowStoreItems[i].cover != nullptr)
        {
            mWindowStoreItems[i].cover->Release();
            mWindowStoreItems[i].cover = nullptr;
        }
    }

    for (int32_t i = 0; i < Context::GetGridCells(); i++)
    {
        mWindowStoreItems[i].appId         = "";
        mWindowStoreItems[i].name          = "";
        mWindowStoreItems[i].author        = "";
        mWindowStoreItems[i].category      = "";
        mWindowStoreItems[i].description   = "";
        mWindowStoreItems[i].latestVersion = "";
        mWindowStoreItems[i].state         = 0;
        mWindowStoreItems[i].cover         = NULL;
    }
    for (int32_t i = 0; i < Context::GetGridCols(); i++)
    {
        mTempStoreItems[i].appId         = "";
        mTempStoreItems[i].name          = "";
        mTempStoreItems[i].author        = "";
        mTempStoreItems[i].category      = "";
        mTempStoreItems[i].description   = "";
        mTempStoreItems[i].latestVersion = "";
        mTempStoreItems[i].state         = 0;
        mTempStoreItems[i].cover         = NULL;
    }
    mCategoryIndex         = 0;
    mWindowStoreItemOffset = 0;
    mWindowStoreItemCount  = 0;
    mCategories.clear();

    if (!LoadCategories())
        return false;

    return RefreshApplications();
}

// ==========================================================================
// Accessors
// ==========================================================================

int32_t StoreManager::GetCategoryCount()
{
    return (int32_t)mCategories.size();
}

int32_t StoreManager::GetCategoryIndex()
{
    return mCategoryIndex;
}

void StoreManager::SetCategoryIndex(int32_t categoryIndex)
{
    if (mCategories.empty() || categoryIndex < 0 || categoryIndex >= (int32_t)mCategories.size())
        return;
    StopIdleWarmer();
    CancelPrefetch();
    mCategoryIndex = categoryIndex;
    RefreshApplications();
}

StoreCategory* StoreManager::GetStoreCategory(int32_t categoryIndex)
{
    if (categoryIndex < 0 || categoryIndex >= (int32_t)mCategories.size())
        return NULL;
    return &mCategories[categoryIndex];
}

int32_t StoreManager::GetSelectedCategoryTotal()
{
    if (mCategories.empty() || mCategoryIndex < 0 || mCategoryIndex >= (int32_t)mCategories.size())
        return 0;
    return mCategories[mCategoryIndex].count;
}

std::string StoreManager::GetSelectedCategoryName()
{
    if (mCategories.empty() || mCategoryIndex < 0 || mCategoryIndex >= (int32_t)mCategories.size())
        return std::string("");
    return mCategories[mCategoryIndex].name;
}

int32_t StoreManager::GetWindowStoreItemOffset()
{
    return mWindowStoreItemOffset;
}

int32_t StoreManager::GetWindowStoreItemCount()
{
    return mWindowStoreItemCount;
}

StoreItem* StoreManager::GetWindowStoreItem(int32_t storeItemIndex)
{
    if (storeItemIndex < 0 || storeItemIndex >= mWindowStoreItemCount)
        return NULL;
    return &mWindowStoreItems[storeItemIndex];
}

bool StoreManager::HasPrevious()
{
    if (mCategories.empty()) return false;
    return mWindowStoreItemOffset >= Context::GetGridCols();
}

bool StoreManager::HasNext()
{
    if (mCategories.empty() || mCategoryIndex < 0 || mCategoryIndex >= (int32_t)mCategories.size())
        return false;
    int32_t categoryItemCount        = mCategories[mCategoryIndex].count;
    int32_t windowStoreItemEndOffset = mWindowStoreItemOffset + mWindowStoreItemCount;
    return windowStoreItemEndOffset < categoryItemCount;
}

// ==========================================================================
// LoadPrevious -- synchronous (going back is less common)
// ==========================================================================

bool StoreManager::LoadPrevious()
{
    if (!HasPrevious())
        return false;

    int32_t newWindowStoreItemOffset = mWindowStoreItemOffset - Context::GetGridCols();
    int32_t loadedCount = 0;

    if (mPrefetchPrevState   == PREFETCH_READY
        && mPrefetchPrevOffset   == newWindowStoreItemOffset
        && mPrefetchPrevCategory == mCategoryIndex
        && mPrefetchPrevCount    >  0)
    {
        // Prefetch hit -- swap in instantly
        Debug::Print("LoadPrevious: prefetch hit offset=%d\n", newWindowStoreItemOffset);
        loadedCount = mPrefetchPrevCount;
        for (int32_t i = 0; i < loadedCount; i++)
            CopyStoreItem(mTempStoreItems[i], mPrefetchPrevItems[i]);

        mPrefetchPrevState  = PREFETCH_IDLE;
        mPrefetchPrevOffset = -1;
        mPrefetchPrevCount  = 0;
    }
    else
    {
        // Prefetch miss -- sync fallback
        Debug::Print("LoadPrevious: prefetch miss, sync fetch offset=%d\n", newWindowStoreItemOffset);
        CancelPrefetch();
        if (!LoadApplications(mTempStoreItems, newWindowStoreItemOffset, Context::GetGridCols(), &loadedCount))
            return false;
    }

    int32_t remainder     = mWindowStoreItemCount % Context::GetGridCols();
    int32_t itemsToRemove = remainder ? remainder : Context::GetGridCols();

    for (int32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[mWindowStoreItemCount - 1 - i];
        if (storeItem.cover != nullptr)
            storeItem.cover->Release();
    }

    for (int32_t i = mWindowStoreItemCount - itemsToRemove; i > 0; i--)
        CopyStoreItem(mWindowStoreItems[i - 1 + loadedCount], mWindowStoreItems[i - 1]);

    for (int32_t i = 0; i < loadedCount; i++)
        CopyStoreItem(mWindowStoreItems[i], mTempStoreItems[i]);

    mWindowStoreItemOffset = newWindowStoreItemOffset;
    mWindowStoreItemCount  = mWindowStoreItemCount - itemsToRemove + loadedCount;

    KickPrefetch();
    return true;
}

// ==========================================================================
// LoadNext -- uses prefetch if ready, falls back to synchronous
// ==========================================================================

bool StoreManager::LoadNext()
{
    if (!HasNext())
        return false;

    int32_t newWindowStoreItemOffset = mWindowStoreItemOffset + Context::GetGridCols();
    int32_t tempOffset               = Context::GetGridCols() * Math::MaxInt32(0, Context::GetGridRows() - 1);
    int32_t fetchOffset              = newWindowStoreItemOffset + tempOffset;

    int32_t loadedCount = 0;

    if (mPrefetchState   == PREFETCH_READY
        && mPrefetchOffset   == fetchOffset
        && mPrefetchCategory == mCategoryIndex
        && mPrefetchCount    >  0)
    {
        // Prefetch hit -- swap in instantly
        Debug::Print("LoadNext: prefetch hit offset=%d\n", fetchOffset);
        loadedCount = mPrefetchCount;
        for (int32_t i = 0; i < loadedCount; i++)
            CopyStoreItem(mTempStoreItems[i], mPrefetchItems[i]);

        mPrefetchState  = PREFETCH_IDLE;
        mPrefetchOffset = -1;
        mPrefetchCount  = 0;
    }
    else
    {
        // Prefetch miss -- sync fallback
        Debug::Print("LoadNext: prefetch miss, sync fetch offset=%d\n", fetchOffset);
        CancelPrefetch();
        if (!LoadApplications(mTempStoreItems, fetchOffset, Context::GetGridCols(), &loadedCount))
            return false;
    }

    int32_t itemsToRemove = Context::GetGridCols();
    for (int32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[i];
        if (storeItem.cover != nullptr)
            storeItem.cover->Release();
    }

    for (int32_t i = itemsToRemove; i < mWindowStoreItemCount; i++)
        CopyStoreItem(mWindowStoreItems[i - itemsToRemove], mWindowStoreItems[i]);

    for (int32_t i = 0; i < loadedCount; i++)
        CopyStoreItem(mWindowStoreItems[mWindowStoreItemCount - itemsToRemove + i], mTempStoreItems[i]);

    mWindowStoreItemOffset = newWindowStoreItemOffset;
    mWindowStoreItemCount  = mWindowStoreItemCount - itemsToRemove + loadedCount;

    // Immediately kick off prefetch for the next row after this one
    KickPrefetch();
    return true;
}

// ==========================================================================
// LoadAtOffset -- jump to any position
// ==========================================================================

bool StoreManager::LoadAtOffset(int32_t offset)
{
    CancelPrefetch();

    for (int32_t i = 0; i < mWindowStoreItemCount; i++)
    {
        if (mWindowStoreItems[i].cover != nullptr)
        {
            mWindowStoreItems[i].cover->Release();
            mWindowStoreItems[i].cover = nullptr;
        }
    }
    for (int32_t i = 0; i < Context::GetGridCells(); i++)
    {
        mWindowStoreItems[i].appId         = "";
        mWindowStoreItems[i].name          = "";
        mWindowStoreItems[i].author        = "";
        mWindowStoreItems[i].category      = "";
        mWindowStoreItems[i].description   = "";
        mWindowStoreItems[i].latestVersion = "";
        mWindowStoreItems[i].state         = 0;
        mWindowStoreItems[i].cover         = NULL;
    }
    mWindowStoreItemCount  = 0;
    mWindowStoreItemOffset = 0;

    int32_t loadedCount = 0;
    if (!LoadApplications(mWindowStoreItems, offset, Context::GetGridCells(), &loadedCount))
        return false;

    mWindowStoreItemOffset = offset;
    mWindowStoreItemCount  = loadedCount;

    KickPrefetch();
    return true;
}

// ==========================================================================
// TryGetStoreVersions
// ==========================================================================

bool StoreManager::TryGetStoreVersions(std::string appId, StoreVersions* storeVersions)
{
    StoreItem* storeItem = NULL;
    int32_t count = GetWindowStoreItemCount();
    for (int32_t i = 0; i < count; i++)
    {
        StoreItem* item = GetWindowStoreItem(i);
        if (item != NULL && item->appId == appId)
        {
            storeItem = item;
            break;
        }
    }

    if (storeItem == NULL)
        return false;

    VersionsResponse versionsResponse;
    if (!WebManager::TryGetVersions(appId, versionsResponse))
        return false;

    if (versionsResponse.versions.empty())
    {
        Debug::Print("TryGetStoreVersions: no versions for %s\n", appId.c_str());
        return false;
    }

    storeVersions->appId         = storeItem->appId;
    storeVersions->name          = versionsResponse.name;
    storeVersions->author        = versionsResponse.author;
    storeVersions->description   = versionsResponse.description;
    storeVersions->latestVersion = versionsResponse.latestVersion;
    storeVersions->cover         = nullptr;
    storeVersions->screenshot    = nullptr;
    storeVersions->versions.clear();

    for (int32_t i = 0; i < (int32_t)versionsResponse.versions.size(); i++)
    {
        VersionItem* versionItem = &versionsResponse.versions[i];

        StoreVersion storeVersion;
        storeVersion.appId                     = storeItem->appId;
        storeVersion.versionId                 = versionItem->id;
        storeVersion.version                   = versionItem->version;
        storeVersion.versionScrollState.active = false;
        storeVersion.size                      = versionItem->size;
        storeVersion.releaseDate               = versionItem->releaseDate;
        storeVersion.changeLog                 = versionItem->changeLog;
        storeVersion.titleId                   = versionItem->titleId;
        storeVersion.region                    = versionItem->region;
        storeVersion.downloadFiles             = versionItem->downloadFiles;
        storeVersion.folderName                = versionItem->folderName;
        storeVersion.state                     = (i == 0) ? storeItem->state : 0;

        if (ViewState::GetViewed(storeItem->appId, storeVersions->latestVersion))
            storeVersion.state = 0;

        std::vector<UserSaveState> userStates;
        if (UserState::TryGetByAppId(storeItem->appId, userStates))
        {
            bool hasInstalled   = false;
            bool hasThisVersion = false;
            for (size_t j = 0; j < userStates.size(); j++)
            {
                const UserSaveState& us = userStates[j];
                if (us.installPath[0] != '\0') {
                    hasInstalled = true;
                    storeVersion.state = 0;
                }
                if (us.versionId[0] != '\0' && storeVersion.versionId == us.versionId)
                    hasThisVersion = true;
            }
            if (hasInstalled && !hasThisVersion)
                storeVersion.state = 2;
        }

        storeVersions->versions.push_back(storeVersion);
    }

    return true;
}

// ==========================================================================
// Private
// ==========================================================================

bool StoreManager::LoadCategories()
{
    mCategories.clear();

    CategoriesResponse categoriesResponse;
    if (!WebManager::TryGetCategories(categoriesResponse))
        return false;

    StoreCategory allApps;
    allApps.name                     = "All Apps";
    allApps.count                    = 0;
    allApps.nameScrollState.active   = false;

    for (int32_t i = 0; i < (int32_t)categoriesResponse.size(); i++)
    {
        StoreCategory storeCategory;
        storeCategory.name                     = categoriesResponse[i].name;
        storeCategory.nameScrollState.active   = false;
        storeCategory.count                    = categoriesResponse[i].count;
        mCategories.push_back(storeCategory);
        allApps.count += categoriesResponse[i].count;
    }

    mCategories.insert(mCategories.begin(), allApps);
    return true;
}

bool StoreManager::LoadApplications(void* dest, int32_t offset, int32_t count, int32_t* loadedCount)
{
    std::string categoryFilter = "";
    if (mCategoryIndex > 0 && mCategoryIndex < (int32_t)mCategories.size())
        categoryFilter = mCategories[mCategoryIndex].name;

    AppsResponse response;
    if (!WebManager::TryGetApps(response, offset, count, categoryFilter, ""))
        return false;

    const int32_t itemCount = (int32_t)response.items.size();
    *loadedCount = itemCount;

    StoreItem* storeItems = (StoreItem*)dest;
    for (int32_t i = 0; i < itemCount; i++)
    {
        AppItem* appItem = &response.items[i];
        storeItems[i].appId                      = appItem->id;
        storeItems[i].name                       = appItem->name;
        storeItems[i].nameScrollState.active     = false;
        storeItems[i].author                     = appItem->author;
        storeItems[i].authorScrollState.active   = false;
        storeItems[i].category                   = appItem->category;
        storeItems[i].description                = appItem->description;
        storeItems[i].latestVersion              = appItem->latestVersion;
        storeItems[i].state                      = ViewState::GetViewed(appItem->id, storeItems[i].latestVersion) ? 0 : appItem->state;
        storeItems[i].cover                      = nullptr;

        std::vector<UserSaveState> userStates;
        if (UserState::TryGetByAppId(storeItems[i].appId, userStates))
        {
            bool hasInstalled     = false;
            bool hasLatestVersion = false;
            for (size_t j = 0; j < userStates.size(); j++)
            {
                const UserSaveState& us = userStates[j];
                if (us.installPath[0] != '\0') {
                    hasInstalled = true;
                    storeItems[i].state = 0;
                }
                if (us.versionId[0] != '\0' && storeItems[i].latestVersion == us.versionId)
                    hasLatestVersion = true;
            }
            if (hasInstalled && !hasLatestVersion)
                storeItems[i].state = 2;
        }
    }

    return true;
}

bool StoreManager::RefreshApplications()
{
    if (!LoadCategories())
        return false;

    for (int32_t i = 0; i < mWindowStoreItemCount; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[i];
        if (storeItem.cover != nullptr)
            storeItem.cover->Release();
    }

    mWindowStoreItemOffset = 0;
    mWindowStoreItemCount  = 0;

    int32_t loadedCount = 0;
    if (!LoadApplications(mWindowStoreItems, 0, Context::GetGridCells(), &loadedCount))
        return false;

    mWindowStoreItemCount = loadedCount;

    KickPrefetch();
    return true;
}