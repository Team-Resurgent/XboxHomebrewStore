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

namespace {
    int32_t mCategoryIndex;
    std::vector<StoreCategory> mCategories;
    int32_t mWindowStoreItemOffset;
    int32_t mWindowStoreItemCount;
    StoreItem* mWindowStoreItems;
    StoreItem* mTempStoreItems;
}

bool StoreManager::Init()
{
    mCategoryIndex = 0;
    mWindowStoreItemOffset = 0;
    mWindowStoreItemCount = 0;
    
    mWindowStoreItems = (StoreItem*)malloc(sizeof(StoreItem) * Context::GetGridCells());
    memset(mWindowStoreItems, 0, sizeof(StoreItem) * Context::GetGridCells());
    mTempStoreItems = (StoreItem*)malloc(sizeof(StoreItem) * Context::GetGridCols());
    memset(mTempStoreItems, 0, sizeof(StoreItem) * Context::GetGridCols());

    if (LoadCategories() == false)
    {
        return false;
    }

    return RefreshApplications();
}

int32_t StoreManager::GetCategoryCount() 
{ 
    return mCategories.size(); 
}

int32_t StoreManager::GetCategoryIndex()
{
    return mCategoryIndex;
}

void StoreManager::SetCategoryIndex(int32_t categoryIndex)
{
    mCategoryIndex = categoryIndex;
    RefreshApplications();
}

StoreCategory* StoreManager::GetStoreCategory(int32_t categoryIndex) 
{ 
    return &mCategories[categoryIndex]; 
}

int32_t StoreManager::GetSelectedCategoryTotal() 
{ 
    return mCategories[mCategoryIndex].count; 
}

std::string StoreManager::GetSelectedCategoryName() 
{ 
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
    return &mWindowStoreItems[storeItemIndex]; 
}

bool StoreManager::HasPrevious()
{
    return mWindowStoreItemOffset >= Context::GetGridCols();
}

bool StoreManager::HasNext()
{
    int32_t categoryItemCount = mCategories[mCategoryIndex].count;
    int32_t windowStoreItemEndOffset = mWindowStoreItemOffset + mWindowStoreItemCount;
    return windowStoreItemEndOffset < categoryItemCount;
}

bool StoreManager::LoadPrevious()
{
    if (HasPrevious() == false)
    {
        return false;
    }

    int32_t newWindowStoreItemOffset = mWindowStoreItemOffset - Context::GetGridCols();

    int32_t loadedCount = 0;
    if (LoadApplications(mTempStoreItems, newWindowStoreItemOffset, Context::GetGridCols(), &loadedCount) == false)
    {
        return false;
    }

    int32_t remainder = mWindowStoreItemCount % Context::GetGridCols();
    int32_t itemsToRemove = remainder ? remainder : Context::GetGridCols();

    for (int32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[mWindowStoreItemCount - 1 - i];
        if (storeItem.cover != nullptr) {
            storeItem.cover->Release();
        }
    }

    for (int32_t i = mWindowStoreItemCount - itemsToRemove; i > 0; i--)
    {
        StoreItem& src = mWindowStoreItems[i - 1];
        StoreItem& dst = mWindowStoreItems[i - 1 + loadedCount];
        dst.appId = src.appId;
        dst.name = src.name;
        dst.nameScrollState = src.nameScrollState;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
        dst.latestVersion = src.latestVersion;
        dst.state = src.state;
        dst.cover = src.cover;
    }

    for (int32_t i = 0; i < loadedCount; i++)
    {
        StoreItem& dst = mWindowStoreItems[i];
        StoreItem& src = mTempStoreItems[i];
        dst.appId = src.appId;
        dst.name = src.name;
        dst.nameScrollState = src.nameScrollState;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
        dst.latestVersion = src.latestVersion;
        dst.state = src.state;
        dst.cover = src.cover;
    }

    mWindowStoreItemOffset = newWindowStoreItemOffset;
    mWindowStoreItemCount = mWindowStoreItemCount - itemsToRemove + loadedCount;
    return true;
}

bool StoreManager::LoadNext()
{
    if (HasNext() == false)
    {
        return false;
    }

    int32_t newWindowStoreItemOffset = mWindowStoreItemOffset + Context::GetGridCols();

    int32_t tempOffset = Context::GetGridCols() * Math::MaxInt32(0, Context::GetGridRows() - 1);

    int32_t loadedCount = 0;
    if (LoadApplications(mTempStoreItems, newWindowStoreItemOffset + tempOffset, Context::GetGridCols(), &loadedCount) == false)
    {
        return false;
    }

    int32_t itemsToRemove = Context::GetGridCols();
    for (int32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[i];
        if (storeItem.cover != nullptr) {
            storeItem.cover->Release();
        }
    }

    for (int32_t i = itemsToRemove; i < mWindowStoreItemCount; i++)
    {
        StoreItem& src = mWindowStoreItems[i];
        StoreItem& dst = mWindowStoreItems[i - itemsToRemove];
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

    for (int32_t i = 0; i < loadedCount; i++)
    {
        StoreItem& dst = mWindowStoreItems[mWindowStoreItemCount - itemsToRemove + i];
        StoreItem& src = mTempStoreItems[i];
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

    mWindowStoreItemOffset = newWindowStoreItemOffset;
    mWindowStoreItemCount = mWindowStoreItemCount - itemsToRemove + loadedCount;
    return true;
}

bool StoreManager::TryGetStoreVersions(std::string appId, StoreVersions* storeVersions)
{
    StoreItem* storeItem = nullptr;
    int32_t count = GetWindowStoreItemCount();
    for (int32_t i = 0; i < count; i++)
    {
        StoreItem* item = GetWindowStoreItem(i);
        if (item != nullptr && item->appId == appId)
        {
            storeItem = item;
            break;
        }
    }
    if (storeItem == nullptr)
    {
        return false;
    }

    VersionsResponse versionsResponse;
    if (WebManager::TryGetVersions(appId, versionsResponse) == false)
    {
        return false;
    }

    storeVersions->appId = storeItem->appId;
    storeVersions->name = versionsResponse.name;
    storeVersions->author =versionsResponse.author;
    storeVersions->description = versionsResponse.description;
    storeVersions->latestVersion = versionsResponse.latestVersion;
    storeVersions->cover = nullptr;
    storeVersions->screenshot = nullptr;

    storeVersions->versions.clear();
    for (int32_t i = 0; i < (int32_t)versionsResponse.versions.size(); i++)
    {
        VersionItem* versionItem = &versionsResponse.versions[i];

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
        if (UserState::TryGetByAppId(storeItem->appId, userStates))
        {
            bool hasInstalled = false;
            bool hasThisVersion = false;
            for (size_t j = 0; j < userStates.size(); j++)
            {
                const UserSaveState& us = userStates[j];
                if (us.installPath[0] != '\0') {
                    hasInstalled = true;
                    storeVersion.state = 0;
                }
                if (us.versionId[0] != '\0' && storeVersion.versionId == us.versionId) {
                    hasThisVersion = true;
                }
            }
            if (hasInstalled && !hasThisVersion)
                storeVersion.state = 2;
        }

        storeVersions->versions.push_back(storeVersion);
    }

    return true;
}

// Private

bool StoreManager::LoadCategories()
{
    mCategories.clear();

    CategoriesResponse categoriesResponse;
    if (!WebManager::TryGetCategories(categoriesResponse))
    {
        return false;
    }
    
    StoreCategory allApps;
    allApps.name = "All Apps";
    allApps.count = 0;

    for (int32_t i = 0; i < (int32_t)categoriesResponse.size(); i++)
    {
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

bool StoreManager::LoadApplications(void* dest, int32_t offset, int32_t count, int32_t* loadedCount)
{
    std::string categoryFilter = mCategoryIndex == 0 ? "" : mCategories[mCategoryIndex].name;

    AppsResponse response;
    if (!WebManager::TryGetApps(response, offset, count, categoryFilter, ""))
    {
        return false;
    }

    const int32_t itemCount = (int32_t)response.items.size();
    *loadedCount = itemCount;

    StoreItem* storeItems = (StoreItem*)dest;
    for (int32_t i = 0; i < itemCount; i++ )
    {
        AppItem* appItem = &response.items[i];
        storeItems[i].appId = appItem->id;
        storeItems[i].name = appItem->name;
        storeItems[i].nameScrollState.active = false;
        storeItems[i].author = appItem->author;
        storeItems[i].authorScrollState.active = false;
        storeItems[i].category = appItem->category;
        storeItems[i].description = appItem->description;
        storeItems[i].latestVersion = appItem->latestVersion;
        storeItems[i].state = ViewState::GetViewed(appItem->id, storeItems[i].latestVersion) ? 0 : appItem->state;
        storeItems[i].cover = nullptr;

        std::vector<UserSaveState> userStates;
        if (UserState::TryGetByAppId(storeItems[i].appId, userStates))
        {
            bool hasInstalled = false;
            bool hasLatestVersion = false;
            for (size_t j = 0; j < userStates.size(); j++)
            {
                const UserSaveState& us = userStates[j];
                if (us.installPath[0] != '\0') {
                    hasInstalled = true;
                    storeItems[i].state = 0;
                }
                if (us.versionId[0] != '\0' && storeItems[i].latestVersion == us.versionId) {
                    hasLatestVersion = true;
                }
            }
            if (hasInstalled && !hasLatestVersion)
                storeItems[i].state = 2;
        }
    }

    return true;
}

bool StoreManager::RefreshApplications()
{
    for (int32_t i = 0; i < mWindowStoreItemCount; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[i];
        if (storeItem.cover != nullptr) {
            storeItem.cover->Release();
        }
    }

    mWindowStoreItemOffset = 0;
    mWindowStoreItemCount = 0;

    int32_t loadedCount = 0;
    if (LoadApplications(mWindowStoreItems, 0, Context::GetGridCells(), &loadedCount) == false)
    {
        return false;
    }
    
    mWindowStoreItemCount = loadedCount;
    return true;
}