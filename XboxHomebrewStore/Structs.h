#pragma once

#include "Main.h"

struct AppItem
{
    std::string id;
    std::string name;
    std::string author;
    std::string category;
    std::string description;
    bool isNew;
};

struct AppsResponse
{
    std::vector<AppItem> items;
    uint32_t page;
    uint32_t pageSize;
    uint32_t totalCount;
    uint32_t totalPages;
    bool hasNextPage;
    bool hasPreviousPage;
};

struct VersionItem
{
    std::string guid;
    std::string version;
    uint32_t size;
    int32_t state;
    std::string release_date;
    std::string changelog;
    std::string title_id;
    std::string region;
};

typedef std::vector<VersionItem> VersionsResponse;

struct CategoryItem
{
    std::string category;
    uint32_t count;
};

typedef std::vector<CategoryItem> CategoriesResponse;
