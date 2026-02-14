#pragma once

#include "Main.h"

class WebManager
{
public:
    typedef void (*DownloadProgressFn)(uint32_t dlNow, uint32_t dlTotal, void* userData);

    static bool Init();
    static bool TryGetFileSize(const std::string& url, uint32_t& outSize);
    static bool TryDownload(const std::string& url, void* buffer, uint32_t bufferSize, DownloadProgressFn progressFn = NULL, void* progressUserData = NULL, volatile bool* pCancelRequested = NULL);
    static bool TryGetApps(std::string& result, uint32_t page, uint32_t pageSize);
    static bool TryGetVersions(const std::string& id, std::string& result);
    static bool TrySyncTime();
};
