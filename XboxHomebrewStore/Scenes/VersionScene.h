#pragma once

#include "Scene.h"

#include "..\Main.h"
#include "..\StoreManager.h"
#include "..\WebManager.h"
#include "..\ImageDownloader.h"

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
    void RenderFailedOverlay();

    void StartDownload();
    static void DownloadProgressCb(uint32_t dlNow, uint32_t dlTotal, void* userData);
    static bool UnpackProgressCb(int currentFile, int totalFiles, const char* currentFileName, void* userData);
    static DWORD WINAPI DownloadThreadProc(LPVOID param);

    ImageDownloader* mImageDownloader;
    StoreVersions mStoreVersions;

    bool mNeedsUpdate;

    bool mSideBarFocused;
    int32_t mHighlightedVersionIndex;
    int32_t mVersionIndex;

    float mListViewScrollOffset;
    float mListViewContentHeight;

    float mDescriptionHeight;
    float mChangeLogHeight;
    int32_t mLastMeasuredVersionIndex;

    /* Download and unpack */
    bool mDownloading;
    volatile bool mDownloadCancelRequested;
    volatile uint32_t mDownloadCurrent;
    volatile uint32_t mDownloadTotal;
    volatile int mProgressIndex;   /* 1-based current (download file or unpack archive) */
    volatile int mProgressCount;   /* total (files to download or zips to unpack) */
    bool mDownloadSuccess;
    bool mShowFailedOverlay;
    HANDLE mDownloadThread;
    bool mUnpacking;
    volatile bool mUnpackCancelRequested;
    volatile int mUnpackCurrent;
    volatile int mUnpackTotal;
};