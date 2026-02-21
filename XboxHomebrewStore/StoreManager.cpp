#include "StoreManager.h"
#include "WebManager.h"
#include "Models.h"
#include "Math.h"
#include "Defines.h"

namespace {
    uint32_t mCategoryIndex;
    std::vector<CategoryItem> mCategories;
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
    
    mWindowStoreItems = (StoreItem*)malloc(sizeof(StoreItem) * STORE_GRID_CELLS);
    memset(mWindowStoreItems, 0, sizeof(StoreItem) * STORE_GRID_CELLS);
    mTempStoreItems = (StoreItem*)malloc(sizeof(StoreItem) * STORE_GRID_COLS);
    memset(mTempStoreItems, 0, sizeof(StoreItem) * STORE_GRID_COLS);

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

CategoryItem* StoreManager::GetCategory(uint32_t categoryIndex) 
{ 
    return &mCategories[categoryIndex]; 
}

uint32_t StoreManager::GetSelectedCategoryTotal() 
{ 
    return mCategories[mCategoryIndex].count; 
}

std::string StoreManager::GetSelectedCategoryName() 
{ 
    return mCategories[mCategoryIndex].category; 
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
    return mWindowStoreItemOffset >= STORE_GRID_COLS;
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

    uint32_t newWindowStoreItemOffset = mWindowStoreItemOffset - STORE_GRID_COLS;

    uint32_t loadedCount = 0;
    if (LoadApplications(mTempStoreItems, newWindowStoreItemOffset, STORE_GRID_COLS, &loadedCount) == false)
    {
        return false;
    }

    uint32_t itemsToRemove = Math::MinInt32(STORE_GRID_COLS, mWindowStoreItemCount);

    for (uint32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[mWindowStoreItemCount - 1 - i];
        if (storeItem.cover != NULL) {
            storeItem.cover->Release();
        }
        if (storeItem.screenshot != NULL) {
            storeItem.screenshot->Release();
        }
    }

    for (uint32_t i = mWindowStoreItemCount - itemsToRemove; i > 0; i--)
    {
        StoreItem& src = mWindowStoreItems[i - 1];
        StoreItem& dst = mWindowStoreItems[i - 1 + loadedCount];
        dst.id = src.id;
        dst.name = src.name;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
        dst.state = src.state;
        dst.cover = src.cover;
        dst.screenshot = src.screenshot;
    }

    for (uint32_t i = 0; i < loadedCount; i++)
    {
        StoreItem& dst = mWindowStoreItems[i];
        StoreItem& src = mTempStoreItems[i];
        dst.id = src.id;
        dst.name = src.name;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
        dst.state = src.state;
        dst.cover = src.cover;
        dst.screenshot = src.screenshot;
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

    uint32_t newWindowStoreItemOffset = mWindowStoreItemOffset + STORE_GRID_COLS;

    uint32_t loadedCount = 0;
    if (LoadApplications(mTempStoreItems, newWindowStoreItemOffset + STORE_GRID_COLS, STORE_GRID_COLS, &loadedCount) == false)
    {
        return false;
    }

    uint32_t itemsToRemove = Math::MinInt32(STORE_GRID_COLS, mWindowStoreItemCount);
    for (uint32_t i = 0; i < itemsToRemove; i++)
    {
        StoreItem& storeItem = mWindowStoreItems[i];
        if (storeItem.cover != NULL) {
            storeItem.cover->Release();
        }
        if (storeItem.screenshot != NULL) {
            storeItem.screenshot->Release();
        }
    }

    for (uint32_t i = itemsToRemove; i < mWindowStoreItemCount; i++)
    {
        StoreItem& src = mWindowStoreItems[i];
        StoreItem& dst = mWindowStoreItems[i - itemsToRemove];
        dst.id = src.id;
        dst.name = src.name;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
        dst.state = src.state;
        dst.cover = src.cover;
        dst.screenshot = src.screenshot;
    }

    for (uint32_t i = 0; i < loadedCount; i++)
    {
        StoreItem& dst = mWindowStoreItems[mWindowStoreItemCount - itemsToRemove + i];
        StoreItem& src = mTempStoreItems[i];
        dst.id = src.id;
        dst.name = src.name;
        dst.author = src.author;
        dst.category = src.category;
        dst.description = src.description;
        dst.state = src.state;
        dst.cover = src.cover;
        dst.screenshot = src.screenshot;
    }

    mWindowStoreItemOffset = newWindowStoreItemOffset;
    mWindowStoreItemCount = mWindowStoreItemCount - itemsToRemove + loadedCount;
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
    
    CategoryItem allApps;
    allApps.category = "All Apps";
    allApps.count = 0;

    for (uint32_t i = 0; i < categoriesResponse.size(); i++)
    {
        CategoryItem item;
        item.category = categoriesResponse[i].category;
        item.count = categoriesResponse[i].count;
        mCategories.push_back(item);

        allApps.count += categoriesResponse[i].count;
    }

    mCategories.insert(mCategories.begin(), allApps);
    return true;
}

bool StoreManager::LoadApplications(void* dest, uint32_t offset, uint32_t count, uint32_t* loadedCount)
{
    std::string categoryFilter = mCategoryIndex == 0 ? "" : mCategories[mCategoryIndex].category;

    AppsResponse response;
    if (!WebManager::TryGetApps(response, offset, count, categoryFilter, ""))
    {
        return false;
    }

    *loadedCount = response.items.size();

    StoreItem* storeItems = (StoreItem*)dest;
    for (uint32_t i = 0; i < response.items.size(); i++ )
    {
        AppItem* appItem = &response.items[i];
        storeItems[i].id = appItem->id;
        storeItems[i].name = appItem->name;
        storeItems[i].author = appItem->author;
        storeItems[i].category = appItem->category;
        storeItems[i].description = appItem->description;
        storeItems[i].state = appItem->state;
        storeItems[i].cover = NULL;
        storeItems[i].screenshot = NULL;
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
        if (storeItem.screenshot != NULL) {
            storeItem.screenshot->Release();
        }
    }

    uint32_t loadedCount = 0;
    if (LoadApplications(mWindowStoreItems, 0, STORE_GRID_CELLS, &loadedCount) == false)
    {
        return false;
    }
    
    mWindowStoreItemOffset = 0;
    mWindowStoreItemCount = loadedCount;
    return true;
}