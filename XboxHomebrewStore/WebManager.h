#pragma once

#include "Main.h"

class WebManager
{
public:
    typedef void (*DownloadProgressFn)(size_t dlNow, size_t dlTotal, void* userData);

    static bool Init();
    static bool TryGetApps(std::string& result, int page, int pageSize);
    static bool TryGetVersions(const std::string& id, std::string& result);
    static bool TryGetFileSize(const std::string& url, size_t& outSize);
    static bool TryDownload(const std::string& url, void* buffer, size_t bufferSize,
                            DownloadProgressFn progressFn = NULL, void* progressUserData = NULL);
};
