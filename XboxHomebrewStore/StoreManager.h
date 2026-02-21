#pragma once

#include "Main.h"
#include "Models.h"

struct StoreItem
{
    std::string id;
    std::string name;
    std::string author;
    std::string category;
    std::string description;
    uint32_t state;

    LPDIRECT3DTEXTURE8 cover;
    LPDIRECT3DTEXTURE8 screenshot;
};


class StoreManager
{
public:
    static bool Init();
    static uint32_t GetCategoryCount();
    static uint32_t GetCategoryIndex();
    static void SetCategoryIndex(uint32_t categoryIndex);
    static CategoryItem* GetCategory(uint32_t categoryIndex);
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
};
