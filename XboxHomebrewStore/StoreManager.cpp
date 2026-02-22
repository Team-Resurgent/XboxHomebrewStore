#include "StoreManager.h"
#include "WebManager.h"
#include "Models.h"
#include "Math.h"
#include "Defines.h"
#include "Context.h"
#include "TextureHelper.h"
#include "ImageDownloader.h"

namespace {
    uint32_t mCategoryIndex;
    std::vector<StoreCategory> mCategories;
    uint32_t mWindowStoreItemOffset;
    uint32_t mWindowStoreItemCount;
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

    return RefreshApplications();;
}

uint32_t StoreManager::GetCategoryCount() 
{ 
    return mCategories.size(); 
}

uint32_t StoreManager::GetCategoryIndex()
{
    return mCategoryIndex;
}

void StoreManager::SetCategoryIndex(uint32_t categoryIndex)
{
    mCategoryIndex = categoryIndex;
    RefreshApplications();
}

StoreCategory* StoreManager::GetStoreCategory(uint32_t categoryIndex) 
{ 
    return &mCategories[categoryIndex]; 
}

uint32_t StoreManager::GetSelectedCategoryTotal() 
{ 
    return mCategories[mCategoryIndex].count; 
}

std::string StoreManager::GetSelectedCategoryName() 
{ 
    return mCategories[mCategoryIndex].name; 
}

uint32_t StoreManager::GetWindowStoreItemOffset()
{
    return mWindowStoreItemOffset;
}

uint32_t StoreManager::GetWindowStoreItemCount()
{
    return mWindowStoreItemCount;
}

StoreItem* StoreManager::GetWindowStoreItem(uint32_t storeItemIndex)
{
    return &mWindowStoreItems[storeItemIndex]; 
}

bool StoreManager::HasPrevious()
{
    return mWindowStoreItemOffset >= Context::GetGridCols();
}

bool StoreManager::HasNext()
{
    uint32_t categoryItemCount = mCategories[mCategoryIndex].count;
    uint32_t windowStoreItemEndOffset = mWindowStoreItemOffset + mWindowStoreItemCount;
    return windowStoreItemEndOffset < categoryItemCount;
}

bool StoreManager::LoadPrevious()
{
    if (HasPrevious() == false)
    {
        return false;
    }

    uint32_t newWindowStoreItemOffset = mWindowStoreItemOffset - Context::GetGridCols();

    uint32_t loadedCount = 0;
    if (LoadApplications(mTempStoreItems, newWindowStoreItemOffset, Context::GetGridCols(), &loadedCount) == false)
    {
        return false;
    }

    uint32_t itemsToRemove = Math::MinInt32(Context::GetGridCols(), mWindowStoreItemCount);

    for (uint32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[mWindowStoreItemCount - 1 - i];
        if (storeItem.cover != NULL) {
            storeItem.cover->Release();
        }
    }

    for (uint32_t i = mWindowStoreItemCount - itemsToRemove; i > 0; i--)
    {
        StoreItem& src = mWindowStoreItems[i - 1];
        StoreItem& dst = mWindowStoreItems[i - 1 + loadedCount];
        dst.appId = src.appId;
        dst.name = src.name;
        dst.nameScrollState = src.nameScrollState;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
        dst.state = src.state;
        dst.cover = src.cover;
    }

    for (uint32_t i = 0; i < loadedCount; i++)
    {
        StoreItem& dst = mWindowStoreItems[i];
        StoreItem& src = mTempStoreItems[i];
        dst.appId = src.appId;
        dst.name = src.name;
        dst.nameScrollState = src.nameScrollState;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
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

    uint32_t newWindowStoreItemOffset = mWindowStoreItemOffset + Context::GetGridCols();

    uint32_t loadedCount = 0;
    if (LoadApplications(mTempStoreItems, newWindowStoreItemOffset + Context::GetGridCols(), Context::GetGridCols(), &loadedCount) == false)
    {
        return false;
    }

    uint32_t itemsToRemove = Math::MinInt32(Context::GetGridCols(), mWindowStoreItemCount);
    for (uint32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[i];
        if (storeItem.cover != NULL) {
            storeItem.cover->Release();
        }
    }

    for (uint32_t i = itemsToRemove; i < mWindowStoreItemCount; i++)
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
        dst.state = src.state;
        dst.cover = src.cover;
    }

    for (uint32_t i = 0; i < loadedCount; i++)
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
        dst.state = src.state;
        dst.cover = src.cover;
    }

    mWindowStoreItemOffset = newWindowStoreItemOffset;
    mWindowStoreItemCount = mWindowStoreItemCount - itemsToRemove + loadedCount;
    return true;
}

bool StoreManager::TryGetStoreVersions(uint32_t storeItemIndex, StoreVersions* storeVersions)
{
    StoreItem* storeItem = GetWindowStoreItem(storeItemIndex);

    VersionsResponse versionsResponse;
    if (WebManager::TryGetVersions(storeItem->appId, versionsResponse) == false)
    {
        return false;
    }

    memset(storeVersions, 0, sizeof(StoreVersions));
    storeVersions->appId = storeItem->appId;
    storeVersions->name = storeItem->name;
    storeVersions->author = storeItem->author;
    storeVersions->description = storeItem->description;

    for (uint32_t i = 0; i < versionsResponse.size(); i++)
    {
        VersionItem* versionItem = &versionsResponse[i];

        StoreVersion storeVersion;
        storeVersion.versionId = versionItem->id;
        storeVersion.version = versionItem->version;
        storeVersion.size = versionItem->size;
        storeVersion.releaseDate = versionItem->releaseDate;
        storeVersion.changeLog = versionItem->changeLog;
        storeVersion.titleId = versionItem->titleId;
        storeVersion.region = versionItem->region;

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

    for (uint32_t i = 0; i < categoriesResponse.size(); i++)
    {
        StoreCategory storeCategory;
        memset(&storeCategory, 0, sizeof(StoreCategory));
        storeCategory.name = categoriesResponse[i].name;
        storeCategory.count = categoriesResponse[i].count;
        mCategories.push_back(storeCategory);

        allApps.count += categoriesResponse[i].count;
    }

    mCategories.insert(mCategories.begin(), allApps);
    return true;
}

bool StoreManager::LoadApplications(void* dest, uint32_t offset, uint32_t count, uint32_t* loadedCount)
{
    std::string categoryFilter = mCategoryIndex == 0 ? "" : mCategories[mCategoryIndex].name;

    AppsResponse response;
    if (!WebManager::TryGetApps(response, offset, count, categoryFilter, ""))
    {
        return false;
    }

    *loadedCount = response.items.size();

    StoreItem* storeItems = (StoreItem*)dest;
    for (uint32_t i = 0; i < response.items.size(); i++ )
    {
        memset(&storeItems[i], 0, sizeof(StoreItem)); 

        AppItem* appItem = &response.items[i];
        storeItems[i].appId = appItem->id;
        storeItems[i].name = appItem->name;
        storeItems[i].author = appItem->author;
        storeItems[i].category = appItem->category;
        storeItems[i].description = appItem->description;
        storeItems[i].state = appItem->state;
    }

    return true;
}

bool StoreManager::RefreshApplications()
{
    for (uint32_t i = 0; i < mWindowStoreItemCount; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[i];
        if (storeItem.cover != NULL) {
            storeItem.cover->Release();
        }
    }

    mWindowStoreItemOffset = 0;
    mWindowStoreItemCount = 0;

    uint32_t loadedCount = 0;
    if (LoadApplications(mWindowStoreItems, 0, Context::GetGridCells(), &loadedCount) == false)
    {
        return false;
    }
    
    mWindowStoreItemCount = loadedCount;
    return true;
}