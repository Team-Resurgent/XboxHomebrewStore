#include "WebManager.h"
#include "String.h"

const std::string store_api_url = "https://192.168.1.89:5001";
const std::string store_app_controller = "/api/apps";
const std::string store_versions = "/versions";

struct BufferWriteContext
{
    char* buffer;
    uint32_t bufferSize;
    uint32_t written;
};

struct ProgressContext
{
    WebManager::DownloadProgressFn fn;
    void* userData;
    volatile bool* pCancelRequested;
};

static size_t StringWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    std::string* out = (std::string*)userdata;
    if (out && total > 0)
    {
        out->append(ptr, total);
    }
    return total;
}

static size_t BufferWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    BufferWriteContext* ctx = (BufferWriteContext*)userdata;
    if (ctx == NULL || ctx->buffer == NULL)
    {
        return 0;
    }
    size_t total = size * nmemb;
    uint32_t space = ctx->bufferSize - ctx->written;
    if (total > (size_t)space)
    {
        return 0;
    }
    memcpy(ctx->buffer + ctx->written, ptr, total);
    ctx->written += (uint32_t)total;
    return total;
}

static int ProgressCallback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    (void)ultotal;
    (void)ulnow;
    ProgressContext* ctx = (ProgressContext*)clientp;
    if (ctx != NULL && ctx->pCancelRequested != NULL && *ctx->pCancelRequested)
    {
        return 1;
    }
    if (ctx != NULL && ctx->fn != NULL)
    {
        ctx->fn((uint32_t)dlnow, (uint32_t)dltotal, ctx->userData);
    }
    return 0;
}

bool WebManager::Init()
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

bool WebManager::TryGetFileSize(const std::string& url, uint32_t& outSize)
{
    outSize = 0;
    if (url.empty())
    {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == NULL)
    {
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    double contentLength = -1;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK || http_code != 200 || contentLength < 0)
    {
        return false;
    }
    outSize = (uint32_t)contentLength;
    return true;
}

bool WebManager::TryDownload(const std::string& url, void* buffer, uint32_t bufferSize, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (url.empty() || buffer == NULL || bufferSize == 0)
    {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == NULL)
    {
        return false;
    }

    BufferWriteContext writeCtx;
    writeCtx.buffer = (char*)buffer;
    writeCtx.bufferSize = bufferSize;
    writeCtx.written = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, BufferWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeCtx);

    ProgressContext progCtx;
    progCtx.fn = progressFn;
    progCtx.userData = progressUserData;
    progCtx.pCancelRequested = pCancelRequested;
    if (progressFn != NULL || pCancelRequested != NULL)
    {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &progCtx);
    }
    else
    {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    }

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && http_code == 200);
}

bool WebManager::TryGetApps(std::string& result, uint32_t page, uint32_t pageSize)
{
    result.clear();
    CURL* curl = curl_easy_init();
    if (curl == NULL)
    {
        return false;
    }

    std::string url = store_api_url + store_app_controller + String::Format("?page=%u&pageSize=%u", page, pageSize);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && http_code == 200);
}

bool WebManager::TryGetVersions(const std::string& id, std::string& result)
{
    result.clear();
    if (id.empty())
    {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == NULL)
    {
        return false;
    }

    std::string url = store_api_url + store_app_controller + "/" + id + store_versions;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && http_code == 200);
}