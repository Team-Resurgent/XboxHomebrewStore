#pragma once

#include "Scene.h"

#include "..\Main.h"
#include "..\StoreManager.h"
#include "..\WebManager.h"

///** Per-version info (no Store types). */
//struct VersionInfo
//{
//    std::string id;
//    std::string version;
//    uint32_t    size;
//    std::string releaseDate;
//    std::string changeLog;
//    std::string titleId;
//    std::string region;
//    int32_t     state;  // STATE_* for display
//};
//
///** Selected app data passed when opening VersionScene. */
//struct SelectedAppInfo
//{
//    std::string              appId;
//    std::string              appName;
//    std::string              author;
//    std::string              description;
//    std::vector<VersionInfo> versions;
//    void*                    pScreenshot;  // D3DTexture* for display (not owned)
//};

class VersionScene : public Scene
{
public:
    explicit VersionScene(const StoreVersions& storeVersions);
    virtual ~VersionScene();
    virtual void Render();
    virtual void Update();

private:
    void RenderHeader();
    void RenderFooter();
    void RenderVersionSidebar();
    void RenderListView();
    void RenderDownloadOverlay();

    void StartDownload();
    static void DownloadProgressCb(uint32_t dlNow, uint32_t dlTotal, void* userData);
    static DWORD WINAPI DownloadThreadProc(LPVOID param);

    StoreVersions mStoreVersions;

    bool mSideBarFocused;
    int32_t mHighlightedVersionIndex;
    int32_t mVersionIndex;

    float mListViewScrollOffset;
    float mListViewContentHeight;

    float mDescriptionHeight;
    float mChangeLogHeight;
    int32_t mLastMeasuredVersionIndex;

    /* Download to HDD0-E: drive */
    bool mDownloading;
    volatile bool mDownloadCancelRequested;
    volatile uint32_t mDownloadNow;
    volatile uint32_t mDownloadTotal;
    bool mDownloadSuccess;
    HANDLE mDownloadThread;
};