#pragma once

#include "Main.h"
#include "Models.h"
#include "Font.h"

typedef struct
{
    std::string appId;
    std::string name;
    ScrollState nameScrollState;
    std::string author;
    ScrollState authorScrollState;
    std::string category;
    std::string description;
    std::string latestVersion;
    uint32_t state;
    D3DTexture* cover;
} StoreItem;

typedef struct
{
    std::string name;
    ScrollState nameScrollState;
    int32_t count;
} StoreCategory;

typedef struct
{
    std::string appId;
    std::string versionId;
    std::string version;
    ScrollState versionScrollState;
    uint32_t size;
    std::string releaseDate;
    std::string changeLog;
    std::string titleId;
    std::string region;
    std::vector<std::string> downloadFiles;
    std::string folderName;
    uint32_t state;
} StoreVersion;

typedef struct
{
    std::string appId;
    std::string name;
    std::string author;
    std::string description;
    std::string latestVersion;
    std::vector<StoreVersion> versions;
    D3DTexture* cover;
    D3DTexture* screenshot;
} StoreVersions;

class StoreManager
{
public:
    static bool Init();
    static bool Reset();
    static int32_t GetCategoryCount();
    static int32_t GetCategoryIndex();
    static void SetCategoryIndex(int32_t categoryIndex);
    static StoreCategory* GetStoreCategory(int32_t categoryIndex);
    static int32_t GetSelectedCategoryTotal();
    static std::string GetSelectedCategoryName();
    static int32_t GetWindowStoreItemOffset();
    static int32_t GetWindowStoreItemCount();
    static StoreItem* GetWindowStoreItem(int32_t storeItemIndex);
    static bool HasPrevious();
    static bool HasNext();
    static bool LoadPrevious();
    static bool LoadNext();
    static bool LoadAtOffset(int32_t offset);
    static bool TryGetStoreVersions(std::string appId, StoreVersions* storeVersions);
    static void CancelPrefetch();      // cancels both next and prev prefetch
    static void StartIdleWarmer();     // begin background idle cover pre-cache
    static void StopIdleWarmer();      // cancel idle warmer thread
    static bool IsIdleWarmerRunning();
private:
    static bool LoadCategories();
    static bool LoadApplications(void* dest, int32_t offset, int32_t count, int32_t* loadedCount);
    static bool RefreshApplications();
    static void KickPrefetch();        // starts background fetch of next AND prev rows
    static DWORD WINAPI PrefetchNextThreadProc(LPVOID param);
    static DWORD WINAPI PrefetchPrevThreadProc(LPVOID param);
    static DWORD WINAPI IdleWarmerThreadProc(LPVOID param);
};