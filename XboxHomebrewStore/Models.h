#pragma once

#include "Main.h"
#include <vector>

struct AppItem
{
    std::string id;
    std::string name;
    std::string author;
    std::string category;
    std::string description;
    uint32_t state;
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
    std::string id;
    std::string version;
    uint32_t size;
    std::string releaseDate;
    std::string changeLog;
    std::string titleId;
    std::string region;

    uint32_t state;
    std::string install_path;  // Local install path (from user state)
};

typedef std::vector<VersionItem> VersionsResponse;

struct CategoryItem
{
    std::string category;
    uint32_t count;
};

typedef std::vector<CategoryItem> CategoriesResponse;
