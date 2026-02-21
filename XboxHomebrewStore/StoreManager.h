#pragma once

#include "Main.h"
#include "Models.h"
#include "Font.h"

struct StoreItem
{
    std::string id;
    std::string name;
    ScrollState nameScrollState;
    std::string author;
    ScrollState authorScrollState;
    std::string category;
    std::string description;
    uint32_t state;
    D3DTexture* cover;
    D3DTexture* screenshot;
};

struct StoreCategory
{
    std::string name;
    ScrollState nameScrollState;
    uint32_t count;
};

class StoreManager
{
public:
    static bool Init();
    static uint32_t GetCategoryCount();
    static uint32_t GetCategoryIndex();
    static void SetCategoryIndex(uint32_t categoryIndex);
    static StoreCategory* GetStoreCategory(uint32_t categoryIndex);
    static uint32_t GetSelectedCategoryTotal();
    static std::string GetSelectedCategoryName();
    static uint32_t GetWindowStoreItemOffset();
    static uint32_t GetWindowStoreItemCount();
    static StoreItem* GetWindowStoreItem(uint32_t storeItemIndex);
    static bool HasPrevious();
    static bool HasNext();
    static bool LoadPrevious();
    static bool LoadNext();
private:
    static bool LoadCategories();
    static bool LoadApplications(void* dest, uint32_t offset, uint32_t count, uint32_t* loadedCount);
    static bool RefreshApplications();
};
