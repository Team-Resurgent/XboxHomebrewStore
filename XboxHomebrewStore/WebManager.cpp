#include "WebManager.h"
#include "String.h"
#include "Debug.h"
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
};

struct MultiSocketContext
{
    curl_socket_t sock;
    int what;
    long timeout_ms;
};

static int MultiTimerCallback(CURLM* multi, long timeout_ms, void* userp)
{
    (void)multi;
    MultiSocketContext* ctx = (MultiSocketContext*)userp;
    if (ctx != nullptr) {
        ctx->timeout_ms = timeout_ms;
    }
    return 0;
}

static int MultiSocketCallback(CURL* easy, curl_socket_t s, int what, void* userp, void* socketp)
{
    (void)easy;
    (void)socketp;
    MultiSocketContext* ctx = (MultiSocketContext*)userp;
    if (ctx != nullptr) {
        if (what == CURL_POLL_REMOVE) {
            ctx->sock = CURL_SOCKET_BAD;
            ctx->what = 0;
        } else {
            ctx->sock = s;
            ctx->what = what;
        }
    }
    return 0;
}

static size_t StringWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    std::string* out = (std::string*)userdata;
    if (out && total > 0) {
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
    if (itemsArr) {
        size_t count = json_array_get_count(itemsArr);
        out.items.reserve(count);
        for (size_t i = 0; i < count; i++)
        {
            JSON_Value* elemVal = json_array_get_value(itemsArr, i);
            JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : nullptr;
            if (itemObj) {
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
    if (!arr) {
        json_value_free(root);
        return false;
    }
    out.clear();
    size_t count = json_array_get_count(arr);
    out.reserve(count);
    for (size_t i = 0; i < count; i++) {
        JSON_Value* elemVal = json_array_get_value(arr, i);
        JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : nullptr;
        if (itemObj) {
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
    if (!arr) {
        json_value_free(root);
        return false;
    }
    out.clear();
    size_t count = json_array_get_count(arr);
    out.reserve(count);
    for (size_t i = 0; i < count; i++) {
        JSON_Value* elemVal = json_array_get_value(arr, i);
        JSON_Object* itemObj = elemVal ? json_value_get_object(elemVal) : nullptr;
        if (itemObj) {
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

static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    ProgressContext* ctx = (ProgressContext*)clientp;
    if (ctx != nullptr && ctx->fn != nullptr) {
        ctx->fn((uint32_t)dlnow, (uint32_t)dltotal, ctx->userData);
    }
    return 0;
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
    {
        char c = headers[i];
        if (c >= 'A' && c <= 'Z') {
            c = c + 32;
        }
        lower += c;
    }

    size_t pos = 0;
    const std::string key = "filename=";

    for (;;) {
        pos = lower.find("content-disposition", pos);
        if (pos == std::string::npos) {
            break;
        }

        size_t fn = lower.find(key, pos);
        if (fn == std::string::npos) {
            pos++;
            continue;
        }

        fn += key.size();

        while (fn < headers.size() &&
               (headers[fn] == ' ' || headers[fn] == '\t'))
        {
            fn++;
        }

        if (fn >= headers.size()) {
            break;
        }

        if (headers[fn] == '"') {
            fn++;
            size_t end = headers.find('"', fn);
            if (end != std::string::npos) {
                outFilename = headers.substr(fn, end - fn);
                return true;
            }
        }

        size_t end = fn;
        while (end < headers.size() &&
               headers[end] != ';' &&
               headers[end] != '\r' &&
               headers[end] != '\n') {
            end++;
        }

        if (end > fn) {
            outFilename = headers.substr(fn, end - fn);

            /* trim trailing space */
            while (!outFilename.empty()) {
                char last = outFilename[outFilename.length() - 1];
                if (last == ' ' || last == '\t') {
                    outFilename.erase(outFilename.length() - 1);
                } else {
                    break;
                }
            }

            return true;
        }

        pos = fn;
    }

    return false;
}


static bool ExtractGoogleConfirmToken(const std::string& html, std::string& token)
{
    size_t pos = html.find("confirm=");
    if (pos == std::string::npos) {
        return false;
    }

    pos += 8;

    size_t end = html.find('&', pos);
    if (end == std::string::npos) {
        end = html.find('"', pos);
    }

    if (end == std::string::npos) {
        return false;
    }

    token = html.substr(pos, end - pos);
    return !token.empty();
}

static bool ExtractGoogleFilename(const std::string& html, std::string& filename)
{
    filename.clear();

    // Find the anchor inside uc-name-size span
    size_t spanPos = html.find("class=\"uc-name-size\"");
    if (spanPos == std::string::npos) {
        return false;
    }

    size_t anchorStart = html.find("<a", spanPos);
    if (anchorStart == std::string::npos) {
        return false;
    }

    anchorStart = html.find(">", anchorStart);
    if (anchorStart == std::string::npos) {
        return false;
    }

    anchorStart++; // move past '>'

    size_t anchorEnd = html.find("</a>", anchorStart);
    if (anchorEnd == std::string::npos) {
        return false;
    }

    filename = html.substr(anchorStart, anchorEnd - anchorStart);

    // Trim whitespace
    while (!filename.empty() &&
          (filename[0] == ' ' || filename[0] == '\n' || filename[0] == '\r'))
        filename.erase(0, 1);

    return !filename.empty();
}

static void RunMultiSocketDownload(CURL* curl, FILE* fp, volatile bool* pCancelRequested, CURLcode* outRes, long* outHttpCode)
{
    ApplyCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    char* effective_url = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    std::string url = std::string(effective_url);

    if (url.find("github.com") != std::string::npos ||
        url.find("objects.githubusercontent.com") != std::string::npos) {
        Debug::Print("Using HTTP/1.0 for GitHub (BearSSL workaround)\n");
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    }

    CURLM* multi = curl_multi_init();
    if (!multi) {
        *outRes = CURLE_OUT_OF_MEMORY;
        *outHttpCode = 0;
        return;
    }

    curl_multi_add_handle(multi, curl);

    MultiSocketContext sockCtx;
    sockCtx.sock = CURL_SOCKET_BAD;
    sockCtx.what = 0;
    sockCtx.timeout_ms = 100;

    curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, MultiSocketCallback);
    curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, &sockCtx);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, MultiTimerCallback);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, &sockCtx);

    int still_running = 0;
    CURLMcode mc = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, &still_running);
    if (mc != CURLM_OK) {
        curl_multi_remove_handle(multi, curl);
        curl_multi_cleanup(multi);
        *outRes = (mc == CURLM_OUT_OF_MEMORY ? CURLE_OUT_OF_MEMORY : CURLE_SEND_ERROR);
        *outHttpCode = 0;
        return;
    }

    CURLcode res = CURLE_OK;

    while (still_running)
    {
        fd_set read_fd, write_fd;
        FD_ZERO(&read_fd);
        FD_ZERO(&write_fd);

        int ev = 0;

        if (sockCtx.sock != CURL_SOCKET_BAD) {
            if (sockCtx.what & CURL_POLL_IN) FD_SET(sockCtx.sock, &read_fd);
            if (sockCtx.what & CURL_POLL_OUT) FD_SET(sockCtx.sock, &write_fd);
        }

        struct timeval tv;
        tv.tv_sec = (sockCtx.timeout_ms < 0 ? 1 : sockCtx.timeout_ms / 1000);
        tv.tv_usec = (sockCtx.timeout_ms < 0 ? 0 : (sockCtx.timeout_ms % 1000) * 1000);

        int r = select(0, &read_fd, &write_fd, NULL, &tv);

        if (pCancelRequested && *pCancelRequested)
        {
            if (sockCtx.sock != CURL_SOCKET_BAD)
            {
                closesocket(sockCtx.sock);
                sockCtx.sock = CURL_SOCKET_BAD;
            }
            curl_multi_remove_handle(multi, curl);
            Debug::Print("Download cancelled instantly.\n");
            res = CURLE_ABORTED_BY_CALLBACK;
            break;
        }

        if (r < 0) {
            res = CURLE_SEND_ERROR;
            break;
        } else if (r == 0) {
            mc = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, &still_running);
        } else if (sockCtx.sock != CURL_SOCKET_BAD) {
            if (FD_ISSET(sockCtx.sock, &read_fd)) ev |= CURL_CSELECT_IN;
            if (FD_ISSET(sockCtx.sock, &write_fd)) ev |= CURL_CSELECT_OUT;
            mc = curl_multi_socket_action(multi, sockCtx.sock, ev, &still_running);
        } else {
            mc = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, &still_running);
        }

        if (mc != CURLM_OK) {
            res = (mc == CURLM_OUT_OF_MEMORY ? CURLE_OUT_OF_MEMORY : CURLE_SEND_ERROR);
            break;
        }

        CURLMsg* msg;
        int msgs_left;
        while ((msg = curl_multi_info_read(multi, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE && msg->easy_handle == curl) {
                res = msg->data.result;
                if (sockCtx.sock != CURL_SOCKET_BAD)
                {
                    closesocket(sockCtx.sock);
                    sockCtx.sock = CURL_SOCKET_BAD;
                }
                curl_multi_remove_handle(multi, curl);
                still_running = 0;
                if (res == CURLE_ABORTED_BY_CALLBACK) {
                    Debug::Print("Download cancelled.\n");
                }
            }
        }
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, outHttpCode);
    *outRes = res;
    curl_multi_remove_handle(multi, curl);
    curl_multi_cleanup(multi);
}

// Applies Content-Disposition rename if present, sets outFinalFileName and file attributes. No curl; caller cleans up.
static bool FinishNormalDownload(const std::string& filePath, const std::string& headers, std::string* outFinalFileName)
{
    std::string detectedName;
    std::string finalPath = filePath;

    if (ParseContentDispositionFilename(headers, detectedName) && !detectedName.empty()) {
        std::string dir;
        size_t slash = filePath.find_last_of("\\/");
        if (slash != std::string::npos) {
            dir = filePath.substr(0, slash + 1);
        }
        finalPath = dir + detectedName;
        if (finalPath != filePath) {
            Debug::Print("Renaming via Content-Disposition\n");
            FileSystem::FileDelete(finalPath);
            MoveFileA(filePath.c_str(), finalPath.c_str());
        }
    }

    if (outFinalFileName) {
        *outFinalFileName = FileSystem::GetFileName(finalPath);
    }
    SetFileAttributesA(finalPath.c_str(), FILE_ATTRIBUTE_NORMAL);
    Debug::Print("=== SUCCESS (normal file) ===\n");
    return true;
}

// Handles HTML response: plain HTML or Google Drive confirm. Reads filePath; for Google confirm, creates its own curl and runs multi-socket download (cancellable).
static bool HandleHtmlResponse(const std::string& filePath, const std::string& url, std::string* outFinalFileName,
    WebManager::DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    std::string html;
    FILE* htmlFile = fopen(filePath.c_str(), "rb");
    if (!htmlFile) {
        return false;
    }
    fseek(htmlFile, 0, SEEK_END);
    long size = ftell(htmlFile);
    fseek(htmlFile, 0, SEEK_SET);
    html.resize(size);
    fread(&html[0], 1, size, htmlFile);
    fclose(htmlFile);

    if (html.find("Google Drive can't scan this file") == std::string::npos) {
        if (outFinalFileName) {
            *outFinalFileName = FileSystem::GetFileName(filePath);
        }
        Debug::Print("HTML but not Google confirm page\n");
        return true;
    }

    Debug::Print("Google large file confirm detected\n");

    std::string realFilename;
    ExtractGoogleFilename(html, realFilename);

    std::string fileId;
    size_t idPos = url.find("id=");
    if (idPos != std::string::npos) {
        fileId = url.substr(idPos + 3);
    }

    std::string confirm, uuid;
    size_t confirmPos = html.find("name=\"confirm\"");
    if (confirmPos != std::string::npos) {
        size_t v = html.find("value=\"", confirmPos);
        if (v != std::string::npos) {
            v += 7;
            confirm = html.substr(v, html.find("\"", v) - v);
        }
    }
    size_t uuidPos = html.find("name=\"uuid\"");
    if (uuidPos != std::string::npos) {
        size_t v = html.find("value=\"", uuidPos);
        if (v != std::string::npos) {
            v += 7;
            uuid = html.substr(v, html.find("\"", v) - v);
        }
    }

    if (fileId.empty() || confirm.empty() || uuid.empty()) {
        Debug::Print("Google confirm extraction FAILED\n");
        return false;
    }

    std::string confirmedUrl =
        "https://drive.usercontent.google.com/download?id=" +
        fileId + "&confirm=" + confirm + "&uuid=" + uuid;

    Debug::Print("Confirmed URL:\n%s\n", confirmedUrl.c_str());

    FileSystem::FileDelete(filePath);

    std::string finalPath = filePath;
    if (!realFilename.empty()) {
        std::string dir;
        size_t slash = filePath.find_last_of("\\/");
        if (slash != std::string::npos) {
            dir = filePath.substr(0, slash + 1);
        }
        finalPath = dir + realFilename;
    }

    FILE* fp = fopen(finalPath.c_str(), "wb");
    if (!fp) {
        return false;
    }
    SetFileAttributesA(finalPath.c_str(), FILE_ATTRIBUTE_ARCHIVE);

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        FileSystem::FileDelete(finalPath);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    ProgressContext progCtx;
    progCtx.fn = progressFn;
    progCtx.userData = progressUserData;
    if (progressFn || pCancelRequested) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progCtx);
    }

    Debug::Print("Starting Google confirm download via curl multi...\n");
    CURLcode res = CURLE_OK;
    long http_code = 0;
    RunMultiSocketDownload(curl, fp, pCancelRequested, &res, &http_code);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        Debug::Print("Google confirmed download failed\n");
        FileSystem::FileDelete(finalPath);
        return false;
    }

    if (outFinalFileName) {
        *outFinalFileName = FileSystem::GetFileName(finalPath);
    }
    SetFileAttributesA(finalPath.c_str(), FILE_ATTRIBUTE_NORMAL);
    Debug::Print("=== SUCCESS (Google large file) ===\n");
    return true;
}

bool WebManager::TryDownload(const std::string url, const std::string filePath, std::string* outFinalFileName, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    Debug::Print("\n=== TryDownload START ===\n");
    Debug::Print("URL: %s\nFilePath: %s\n", url.c_str(), filePath.c_str());

    if (outFinalFileName) {
        outFinalFileName->clear();
    }

    if (url.empty() || filePath.empty()) {
        Debug::Print("FAILED: URL or filePath empty\n");
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        Debug::Print("FAILED: curl_easy_init returned NULL\n");
        return false;
    }

    std::string headers;
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCaptureCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

    ProgressContext progCtx;
    progCtx.fn = progressFn;
    progCtx.userData = progressUserData;
    if (progressFn || pCancelRequested) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progCtx);
    }

    FILE* fp = fopen(filePath.c_str(), "wb");
    if (!fp) {
        Debug::Print("FAILED: fopen failed\n");
        curl_easy_cleanup(curl);
        return false;
    }
    SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_ARCHIVE);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    Debug::Print("Starting download via curl multi...\n");

    CURLcode res = CURLE_OK;
    long http_code = 0;
    RunMultiSocketDownload(curl, fp, pCancelRequested, &res, &http_code);
    fclose(fp);

    Debug::Print("CURLcode: %d\n", res);
    Debug::Print("HTTP_CODE: %ld\n", http_code);
    Debug::Print("CURL error: %s\n", curl_easy_strerror(res));

    if (res != CURLE_OK || http_code != 200) {
        Debug::Print("Download failed — deleting file\n");
        FileSystem::FileDelete(filePath);
        curl_easy_cleanup(curl);
        return false;
    }

    char* ct = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
    std::string contentType = ct ? ct : "";

    Debug::Print("Content-Type: %s\n", contentType.c_str());

    curl_easy_cleanup(curl);

    if (contentType.find("text/html") == std::string::npos) {
        return FinishNormalDownload(filePath, headers, outFinalFileName);
    }

    Debug::Print("HTML detected — checking for Google confirm page\n");
    return HandleHtmlResponse(filePath, url, outFinalFileName, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryGetApps(AppsResponse& result, int32_t offset, int32_t count, const std::string category, const std::string name)
{
    result.items.clear();

    std::string url = store_api_url + store_app_controller +
        String::Format("?offset=%u&count=%u", offset, count);

    if (!category.empty()) {
        url += "&category=" + category;
    }

    if (!name.empty()) {
        url += "&name=" + name;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    ApplyCommonOptions(curl);

    std::string raw;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
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

    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    ApplyCommonOptions(curl);

    std::string raw;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        return false;
    }

    return ParseCategoriesResponse(raw, result);
}

bool WebManager::TryGetVersions(const std::string id, VersionsResponse& result)
{
    result.clear();

    std::string url = store_api_url + store_app_controller + "/" + id + store_versions;

    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    ApplyCommonOptions(curl);

    std::string raw;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        return false;
    }

    return ParseVersionsResponse(raw, result);
}


// bool WebManager::TryDownload(const std::string url, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
// {
//     if (url.empty() || filePath.empty()) {
//         return false;
//     }

//     FILE* fp = fopen(filePath.c_str(), "wb");
//     if (fp == nullptr) {
//         return false;
//     }
//     SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_ARCHIVE);

//     // Use a dedicated handle for downloads
//     CURL* curl = curl_easy_init();
//     if (curl == nullptr) {
//         fclose(fp);
//         FileSystem::FileDelete(filePath);
//         return false;
//     }

//     ApplyCommonOptions(curl);
//     curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//     curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
//     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t (*)(void*, size_t, size_t, void*))fwrite);
//     curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

//     // Set larger file write buffer via setvbuf for fewer disk writes
//     char* fileBuf = (char*)malloc(65536);
//     if (fileBuf != nullptr) {
//         setvbuf(fp, fileBuf, _IOFBF, 65536);
//     }

//     ProgressContext progCtx;
//     progCtx.fn = progressFn;
//     progCtx.userData = progressUserData;
//     progCtx.pCancelRequested = pCancelRequested;
//     if (progressFn != nullptr || pCancelRequested != nullptr) {
//         curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
//         curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
//         curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &progCtx);
//     } else {
//         curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
//     }

//     long http_code = 0;
//     CURLcode res = curl_easy_perform(curl);
//     if (res == CURLE_OK) {
//         curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
//     }

//     fclose(fp);
//     curl_easy_cleanup(curl);

//     if (fileBuf != nullptr) {
//         free(fileBuf);
//     }

//     if (res != CURLE_OK || http_code != 200) {
//         FileSystem::FileDelete(filePath);
//         return false;
//     }

//     SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_NORMAL);
//     return true;
// }

bool WebManager::TryDownloadCover(const std::string id, int32_t width, int32_t height, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty()) {
        return false;
    }
    std::string url = store_api_url + "/api/Cover/" + id + String::Format("?width=%u&height=%u", width, height);
    return TryDownload(url, filePath, nullptr, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryDownloadScreenshot(const std::string id, int32_t width, int32_t height, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty()) {
        return false;
    }
    std::string url = store_api_url + "/api/Screenshot/" + id + String::Format("?width=%u&height=%u", width, height);
    return TryDownload(url, filePath, nullptr, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryDownloadApp(const std::string id, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (id.empty()) {
        return false;
    }
    std::string url = store_api_url + "/api/Download/" + id;
    return TryDownload(url, filePath, nullptr, progressFn, progressUserData, pCancelRequested);
}

bool WebManager::TryDownloadVersionFile(const std::string versionId, int32_t fileIndex, const std::string filePath, DownloadProgressFn progressFn, void* progressUserData, volatile bool* pCancelRequested)
{
    if (versionId.empty()) {
        return false;
    }
    std::string url = store_api_url + "/api/Download/" + versionId + String::Format("?fileIndex=%d", fileIndex);
    return TryDownload(url, filePath, nullptr, progressFn, progressUserData, pCancelRequested);
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