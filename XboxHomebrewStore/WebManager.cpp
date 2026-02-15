#include "WebManager.h"
#include "String.h"
#include "parson.h"
#include "JsonHelper.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

const std::string store_api_url = "https://192.168.1.89:5001";
const std::string store_app_controller = "/api/apps";
const std::string store_versions = "/versions";
const std::string store_categories = "/api/categories";

static const char* ntp_server = "167.160.187.12";
static const int ntp_port = 123;
static const unsigned long ntp_epoch_offset = 2208988800UL;

extern "C" {
    LONG WINAPI NtSetSystemTime(LPFILETIME SystemTime, LPFILETIME PreviousTime);
}

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

static bool ParseAppsResponse(const std::string& raw, AppsResponse& out)
{
    JSON_Value* root = json_parse_string(raw.c_str());
    if (!root) return false;
    JSON_Object* obj = json_value_get_object(root);
    if (!obj)
    {
        json_value_free(root);
        return false;
    }
    out.items.clear();
    JSON_Value* itemsVal = JsonHelper::GetObjectMember(obj, "items");
    JSON_Array* itemsArr = itemsVal ? json_value_get_array(itemsVal) : NULL;
    if (itemsArr)
    {
        size_t count = json_array_get_count(itemsArr);
        for (size_t i = 0; i < count; i++)
        {
            JSON_Value* elemVal = json_array_get_value(itemsArr, i);
            JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : NULL;
            if (itemObj)
            {
                AppItem app;
                app.id = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "id"));
                app.name = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "name"));
                app.author = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "author"));
                app.category = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "category"));
                app.description = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "description"));
                app.isNew = JsonHelper::ToBool(JsonHelper::GetObjectMember(itemObj, "is_new"));
                out.items.push_back(app);
            }
        }
    }
    out.page = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(obj, "page"));
    out.pageSize = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(obj, "pageSize"));
    out.totalCount = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(obj, "totalCount"));
    out.totalPages = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(obj, "totalPages"));
    out.hasNextPage = JsonHelper::ToBool(JsonHelper::GetObjectMember(obj, "hasNextPage"));
    out.hasPreviousPage = JsonHelper::ToBool(JsonHelper::GetObjectMember(obj, "hasPreviousPage"));
    json_value_free(root);
    return true;
}

static bool ParseCategoriesResponse(const std::string& raw, CategoriesResponse& out)
{
    JSON_Value* root = json_parse_string(raw.c_str());
    if (!root) return false;
    JSON_Array* arr = json_value_get_array(root);
    if (!arr)
    {
        json_value_free(root);
        return false;
    }
    out.clear();
    size_t count = json_array_get_count(arr);
    for (size_t i = 0; i < count; i++)
    {
        JSON_Value* elemVal = json_array_get_value(arr, i);
        JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : NULL;
        if (itemObj)
        {
            CategoryItem cat;
            cat.category = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "category"));
            cat.count = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(itemObj, "count"));
            out.push_back(cat);
        }
    }
    json_value_free(root);
    return true;
}

static bool ParseVersionsResponse(const std::string& raw, VersionsResponse& out)
{
    JSON_Value* root = json_parse_string(raw.c_str());
    if (!root) return false;
    JSON_Array* arr = json_value_get_array(root);
    if (!arr)
    {
        json_value_free(root);
        return false;
    }
    out.clear();
    size_t count = json_array_get_count(arr);
    for (size_t i = 0; i < count; i++)
    {
        JSON_Value* elemVal = json_array_get_value(arr, i);
        JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : NULL;
        if (itemObj)
        {
            VersionItem ver;
            ver.guid = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "guid"));
            ver.version = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "version"));
            ver.size = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(itemObj, "size"));
            ver.state = JsonHelper::ToInt(JsonHelper::GetObjectMember(itemObj, "state"));
            ver.releaseDate = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "release_date"));
            ver.changeLog = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "changelog"));
            ver.titleId = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "title_id"));
            ver.region = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "region"));
            out.push_back(ver);
        }
    }
    json_value_free(root);
    return true;
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

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (res == CURLE_OK)
        {
            double contentLength = -1;
            res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
            if (res == CURLE_OK)
            {
                outSize = (uint32_t)contentLength;
            }
        }
    }

    curl_easy_cleanup(curl);
    return (res == CURLE_OK && http_code == 200);
}

bool WebManager::TryDownload(const std::string& url, const std::string& filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (url.empty() || filePath.empty())
    {
        return false;
    }
    FILE* fp = fopen(filePath.c_str(), "wb");
    if (fp == NULL)
    {
        return false;
    }

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t (*)(void*, size_t, size_t, void*))fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

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

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200)
    {
        remove(filePath.c_str());
        return false;
    }
    return true;
}

bool WebManager::TryGetApps(AppsResponse& result, uint32_t page, uint32_t pageSize, const std::string& category, const std::string& name)
{
    result.items.clear();
    std::string url = store_api_url + store_app_controller + String::Format("?page=%u&pageSize=%u", page, pageSize);
    if (!category.empty())
    {
        url += "&category=" + category;
    }
    if (!name.empty())
    {
        url += "&name=" + name;
    }

    std::string raw;
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_easy_cleanup(curl);
    if (res != CURLE_OK || http_code != 200) {
        return false;
    }
    return ParseAppsResponse(raw, result);
}

bool WebManager::TryGetCategories(CategoriesResponse& result)
{
    result.clear();
    std::string url = store_api_url + store_categories;

    std::string raw;
    CURL* curl = curl_easy_init();
    if (curl == NULL)
    {
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        return false;
    }
    return ParseCategoriesResponse(raw, result);
}

bool WebManager::TryGetVersions(const std::string& id, VersionsResponse& result)
{
    result.clear();
    std::string url = store_api_url + store_app_controller + "/" + id + store_versions;

    std::string raw;
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        return false;
    }
    return ParseVersionsResponse(raw, result);
}

bool WebManager::TryDownloadCover(const std::string& id, uint32_t width, uint32_t height, const std::string& filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty())
    {
        return false;
    }
    std::string url = store_api_url + "/api/Cover/" + id + String::Format("?width=%u&height=%u", width, height);
    return TryDownload(url, filePath, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryDownloadScreenshot(const std::string& id, uint32_t width, uint32_t height, const std::string& filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty())
    {
        return false;
    }
    std::string url = store_api_url + "/api/Screenshot/" + id + String::Format("?width=%u&height=%u", width, height);
    return TryDownload(url, filePath, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TrySyncTime()
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ntp_port);
    addr.sin_addr.s_addr = inet_addr(ntp_server);

    unsigned char packet[48];
    memset(packet, 0, 48);
    packet[0] = 0x1B;

    sendto(sock, (const char*)packet, sizeof(packet), 0, (sockaddr*)&addr, sizeof(addr));

    int addrLen = sizeof(addr);
    int recvLen = recvfrom(sock, (char*)packet, sizeof(packet), 0, (sockaddr*)&addr, &addrLen);
    if (recvLen < 0) {
        return false;
    }

    uint32_t seconds;
    memcpy(&seconds, &packet[40], sizeof(seconds));
    seconds = ntohl(seconds);

    time_t unixTime = seconds - ntp_epoch_offset;

    FILETIME ft;
    LONGLONG ll = Int32x32To64(unixTime, 10000000) + 116444736000000000LL;
    ft.dwLowDateTime = (DWORD)ll;
    ft.dwHighDateTime = (DWORD)(ll >> 32);
    NtSetSystemTime(&ft, NULL);

    closesocket(sock);
    return true;
}
