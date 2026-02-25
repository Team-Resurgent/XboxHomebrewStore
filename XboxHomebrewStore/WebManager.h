#pragma once

#include "Main.h"
#include "Models.h"

class WebManager
{
public:
    typedef void (*DownloadProgressFn)(uint32_t dlNow, uint32_t dlTotal, void* userData);

    static bool Init();
    static bool TryDownload(const std::string url, const std::string filePath, std::string* outFinalFileName = nullptr, DownloadProgressFn progressFn = nullptr, void* progressUserData = nullptr, volatile bool* pCancelRequested = nullptr);
    static bool TryDownloadCover(const std::string id, int32_t width, int32_t height, const std::string filePath, DownloadProgressFn progressFn = nullptr, void* progressUserData = nullptr, volatile bool* pCancelRequested = nullptr);
    static bool TryDownloadScreenshot(const std::string id, int32_t width, int32_t height, const std::string filePath, DownloadProgressFn progressFn = nullptr, void* progressUserData = nullptr, volatile bool* pCancelRequested = nullptr);
    static bool TryDownloadApp(const std::string id, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested);
    static bool TryDownloadVersionFile(const std::string versionId, int32_t fileIndex, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested);
    static bool TryGetApps(AppsResponse& result, int32_t offset, int32_t count, const std::string category = "", const std::string name = "");
    static bool TryGetCategories(CategoriesResponse& result);
    static bool TryGetVersions(const std::string id, VersionsResponse& result);
    static bool TrySyncTime();
};
