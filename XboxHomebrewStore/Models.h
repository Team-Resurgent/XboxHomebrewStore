#pragma once

#include "Main.h"

typedef struct
{
    std::string id;
    std::string name;
    std::string author;
    std::string category;
    std::string description;
    int32_t state;
} AppItem;

typedef struct
{
    std::vector<AppItem> items;
} AppsResponse;

typedef struct
{
    std::string id;
    std::string version;
    uint32_t size;
    std::string releaseDate;
    std::string changeLog;
    std::string titleId;
    std::string region;
} VersionItem;

typedef std::vector<VersionItem> VersionsResponse;

typedef struct {
    std::string name;
    int32_t count;
} CategoryItem;

typedef std::vector<CategoryItem> CategoriesResponse;
