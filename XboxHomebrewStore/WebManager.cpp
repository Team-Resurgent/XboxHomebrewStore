#include "WebManager.h"
#include "String.h"
#include "parson.h"
#include "JsonHelper.h"
#include "FileSystem.h"
const std::string store_api_url = "https://192.168.1.98:5001";
const std::string store_app_controller = "/api/apps";
const std::string store_versions = "/versions";
const std::string store_categories = "/api/categories";

static const char* ntp_server = "167.160.187.12";
static const int32_t ntp_port = 123;
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

// Persistent curl handle for connection reuse (avoids repeated TCP/SSL handshake)
static CURL* s_persistentCurl = nullptr;

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

static bool ParseAppsResponse(const std::string raw, AppsResponse& out)
{
    JSON_Value* root = json_parse_string(raw.c_str());
    if (!root) {
        return false;
    }

    out.items.clear();

    JSON_Array* itemsArr = json_value_get_array(root);
    if (itemsArr)
    {
        size_t count = json_array_get_count(itemsArr);
        out.items.reserve(count);
        for (size_t i = 0; i < count; i++)
        {
            JSON_Value* elemVal = json_array_get_value(itemsArr, i);
            JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : nullptr;
            if (itemObj)
            {
                AppItem app;
                app.id = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "id"));
                app.name = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "name"));
                app.author = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "author"));
                app.category = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "category"));
                app.description = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "description"));
                app.latestVersion = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "latest_version"));
                app.state = JsonHelper::ToBool(JsonHelper::GetObjectMember(itemObj, "state"));
                out.items.push_back(app);
            }
        }
    }

    json_value_free(root);
    return true;
}

static bool ParseCategoriesResponse(const std::string raw, CategoriesResponse& out)
{
    JSON_Value* root = json_parse_string(raw.c_str());
    if (!root) {
        return false;
    }
    JSON_Array* arr = json_value_get_array(root);
    if (!arr)
    {
        json_value_free(root);
        return false;
    }
    out.clear();
    size_t count = json_array_get_count(arr);
    out.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        JSON_Value* elemVal = json_array_get_value(arr, i);
        JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : nullptr;
        if (itemObj)
        {
            CategoryItem cat;
            cat.name = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "name"));
            cat.count = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(itemObj, "count"));
            out.push_back(cat);
        }
    }
    json_value_free(root);
    return true;
}

static bool ParseVersionsResponse(const std::string raw, VersionsResponse& out)
{
    JSON_Value* root = json_parse_string(raw.c_str());
    if (!root) {
        return false;
    }
    JSON_Array* arr = json_value_get_array(root);
    if (!arr)
    {
        json_value_free(root);
        return false;
    }
    out.clear();
    size_t count = json_array_get_count(arr);
    out.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        JSON_Value* elemVal = json_array_get_value(arr, i);
        JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : nullptr;
        if (itemObj)
        {
            VersionItem ver;
            ver.id = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "id"));
            ver.version = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "version"));
            ver.size = JsonHelper::ToUInt32(JsonHelper::GetObjectMember(itemObj, "size"));
            ver.releaseDate = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "release_date"));
            ver.changeLog = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "changelog"));
            ver.titleId = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "title_id"));
            ver.region = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "region"));
            ver.downloadFiles = JsonHelper::ToStringArray(JsonHelper::GetObjectMember(itemObj, "download_files"));
            ver.folderName = JsonHelper::ToString(JsonHelper::GetObjectMember(itemObj, "folder_name"));
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
    if (ctx != nullptr && ctx->pCancelRequested != nullptr && *ctx->pCancelRequested)
    {
        return 1;
    }
    if (ctx != nullptr && ctx->fn != nullptr)
    {
        ctx->fn((uint32_t)dlnow, (uint32_t)dltotal, ctx->userData);
    }
    return 0;
}

// Helper: get or create persistent curl handle
static CURL* GetCurl()
{
    if (s_persistentCurl != nullptr)
    {
        curl_easy_reset(s_persistentCurl);
        return s_persistentCurl;
    }
    s_persistentCurl = curl_easy_init();
    return s_persistentCurl;
}

// Helper: apply common performance options to a curl handle
static void ApplyCommonOptions(CURL* curl)
{
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 65536L);       // 64KB receive buffer (default is 16KB)
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);          // Disable Nagle's algorithm
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
}

bool WebManager::Init()
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

bool WebManager::TryGetFileSize(const std::string url, uint32_t& outSize)
{
    outSize = 0;

    CURL* curl = GetCurl();
    if (curl == nullptr) {
        return false;
    }

    ApplyCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

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

    return (res == CURLE_OK && http_code == 200);
}

static size_t HeaderCaptureCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    std::string* headers = (std::string*)userdata;
    if (headers && total > 0) {
        headers->append(ptr, total);
    }
    return total;
}

static bool ParseContentDispositionFilename(const std::string& headers, std::string& outFilename)
{
    /* Look for filename= in Content-Disposition (case-insensitive). */
    std::string lower;
    lower.reserve(headers.size());
    for (size_t i = 0; i < headers.size(); i++)
        lower += (char)(headers[i] >= 'A' && headers[i] <= 'Z' ? headers[i] + 32 : headers[i]);
    size_t pos = 0;
    const std::string key = "filename=";
    for (;;) {
        pos = lower.find("content-disposition", pos);
        if (pos == std::string::npos) break;
        size_t fn = lower.find(key, pos);
        if (fn == std::string::npos) { pos++; continue; }
        fn += key.size();
        while (fn < headers.size() && (headers[fn] == ' ' || headers[fn] == '\t')) fn++;
        if (fn >= headers.size()) break;
        if (headers[fn] == '"') {
            fn++;
            size_t end = headers.find('"', fn);
            if (end != std::string::npos) {
                outFilename = headers.substr(fn, end - fn);
                return true;
            }
        }
        size_t end = fn;
        while (end < headers.size() && headers[end] != ';' && headers[end] != '\r' && headers[end] != '\n') end++;
        if (end > fn) {
            outFilename = headers.substr(fn, end - fn);
            /* trim trailing space */
            while (!outFilename.empty() && (outFilename.back() == ' ' || outFilename.back() == '\t')) outFilename.pop_back();
            return true;
        }
        pos = fn;
    }
    return false;
}

bool WebManager::TryGetDownloadFilename(const std::string url, std::string& outFilename)
{
    outFilename.clear();
    if (url.empty()) return false;

    CURL* curl = curl_easy_init();
    if (curl == nullptr) return false;

    std::string headers;
    ApplyCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCaptureCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return false;

    if (ParseContentDispositionFilename(headers, outFilename)) return true;

    /* Fallback: last path segment of URL */
    size_t slash = url.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < url.size()) {
        size_t q = url.find('?', slash + 1);
        if (q != std::string::npos)
            outFilename = url.substr(slash + 1, q - (slash + 1));
        else
            outFilename = url.substr(slash + 1);
    } else {
        outFilename = url;
    }
    return true;
}

bool WebManager::TryDownload(const std::string url, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (url.empty() || filePath.empty()) {
        return false;
    }

    FILE* fp = fopen(filePath.c_str(), "wb");
    if (fp == nullptr) {
        return false;
    }
    SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_ARCHIVE);

    // Use a dedicated handle for downloads
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        fclose(fp);
        FileSystem::FileDelete(filePath);
        return false;
    }

    ApplyCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t (*)(void*, size_t, size_t, void*))fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    // Set larger file write buffer via setvbuf for fewer disk writes
    char* fileBuf = (char*)malloc(65536);
    if (fileBuf != nullptr) {
        setvbuf(fp, fileBuf, _IOFBF, 65536);
    }

    ProgressContext progCtx;
    progCtx.fn = progressFn;
    progCtx.userData = progressUserData;
    progCtx.pCancelRequested = pCancelRequested;
    if (progressFn != nullptr || pCancelRequested != nullptr) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &progCtx);
    } else {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    }

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    fclose(fp);
    curl_easy_cleanup(curl);

    if (fileBuf != nullptr) {
        free(fileBuf);
    }

    if (res != CURLE_OK || http_code != 200) {
        FileSystem::FileDelete(filePath);
        return false;
    }

    SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_NORMAL);
    return true;
}

bool WebManager::TryGetApps(AppsResponse& result, int32_t offset, int32_t count, const std::string category, const std::string name)
{
    result.items.clear();
    std::string url = store_api_url + store_app_controller + String::Format("?offset=%u&count=%u", offset, count);
    if (!category.empty())
    {
        url += "&category=" + category;
    }
    if (!name.empty())
    {
        url += "&name=" + name;
    }

    std::string raw;
    CURL* curl = GetCurl();
    if (curl == nullptr) {
        return false;
    }

    ApplyCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

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
    CURL* curl = GetCurl();
    if (curl == nullptr) {
        return false;
    }

    ApplyCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    if (res != CURLE_OK || http_code != 200) {
        return false;
    }
    return ParseCategoriesResponse(raw, result);
}

bool WebManager::TryGetVersions(const std::string id, VersionsResponse& result)
{
    result.clear();
    std::string url = store_api_url + store_app_controller + "/" + id + store_versions;

    std::string raw;
    CURL* curl = GetCurl();
    if (curl == nullptr) {
        return false;
    }

    ApplyCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    if (res != CURLE_OK || http_code != 200) {
        return false;
    }
    return ParseVersionsResponse(raw, result);
}

bool WebManager::TryDownloadCover(const std::string id, int32_t width, int32_t height, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty())
    {
        return false;
    }
    std::string url = store_api_url + "/api/Cover/" + id + String::Format("?width=%u&height=%u", width, height);
    return TryDownload(url, filePath, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryDownloadScreenshot(const std::string id, int32_t width, int32_t height, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty())
    {
        return false;
    }
    std::string url = store_api_url + "/api/Screenshot/" + id + String::Format("?width=%u&height=%u", width, height);
    return TryDownload(url, filePath, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryDownloadApp(const std::string id, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty())
    {
        return false;
    }
    std::string url = store_api_url + "/api/Download/" + id;
    return TryDownload(url, filePath, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryDownloadVersionFile(const std::string versionId, int32_t fileIndex, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (versionId.empty())
    {
        return false;
    }
    std::string url = store_api_url + "/api/Download/" + versionId + String::Format("?fileIndex=%d", fileIndex);
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
        closesocket(sock);  // Fixed: was leaking socket on error
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
    NtSetSystemTime(&ft, nullptr);

    closesocket(sock);
    return true;
}