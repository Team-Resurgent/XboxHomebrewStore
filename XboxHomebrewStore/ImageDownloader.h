//=============================================================================
// ImageDownloader.h - Threaded queue for cover/screenshot image downloads
//=============================================================================

#pragma once

#include "Main.h"

enum ImageDownloadType
{
    IMAGE_COVER,
    IMAGE_SCREENSHOT
};

class ImageDownloader
{
public:
    ImageDownloader();
    ~ImageDownloader();

    void Queue(D3DTexture** pOutTexture, const std::string appId, ImageDownloadType type);
    void CancelAll();


    static std::string GetCoverCachePath( const std::string appId );
    static bool IsCoverCached( const std::string appId );
    static std::string GetScreenshotCachePath( const std::string appId );
    static bool IsScreenshotCached( const std::string appId );

private:

    struct Request
    {
        D3DTexture** pOutTexture;
        std::string appId;
        ImageDownloadType type;
    };

    static DWORD WINAPI ThreadProc( LPVOID param );
    void WorkerLoop();

    std::deque<Request>    m_queue;
    CRITICAL_SECTION       m_queueLock;
    HANDLE                 m_thread;
    volatile bool          m_quit;
    volatile bool          m_cancelRequested;
};
