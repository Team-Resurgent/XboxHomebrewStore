#pragma once

#include "Main.h"
#include "Models.h"

class WebManager
{
public:
    typedef void (*DownloadProgressFn)(uint32_t dlNow, uint32_t dlTotal, void* userData);

    static bool Init();
	static void Cleanup();
    static bool TryGetFileSize(const std::string& url, uint32_t& outSize);
    static bool TryDownload(const std::string& url, const std::string& filePath, DownloadProgressFn progressFn = NULL, void* progressUserData = NULL, volatile bool* pCancelRequested = NULL);
    static bool TryDownloadCover(const std::string& id, uint32_t width, uint32_t height, const std::string& filePath, DownloadProgressFn progressFn = NULL, void* progressUserData = NULL, volatile bool* pCancelRequested = NULL);
    static bool TryDownloadScreenshot(const std::string& id, uint32_t width, uint32_t height, const std::string& filePath, DownloadProgressFn progressFn = NULL, void* progressUserData = NULL, volatile bool* pCancelRequested = NULL);
    static bool TryDownloadApp(const std::string& id, const std::string& filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested);
    static bool TryGetApps(AppsResponse& result, uint32_t page, uint32_t pageSize, const std::string& category = "", const std::string& name = "");
    static bool TryGetCategories(CategoriesResponse& result);
    static bool TryGetVersions(const std::string& id, VersionsResponse& result);
    static bool TrySyncTime();
};
