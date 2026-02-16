//=============================================================================
// ImageDownloader.h - Threaded queue for cover/screenshot image downloads
//=============================================================================

#pragma once

#include "Main.h"
#include <string>
#include <deque>

struct IDirect3DTexture8;
typedef struct IDirect3DTexture8* LPDIRECT3DTEXTURE8;
struct IDirect3DDevice8;
typedef struct IDirect3DDevice8* LPDIRECT3DDEVICE8;

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

    // Queue a cover or screenshot download. pOutTexture must stay valid until ProcessCompleted runs for this request (or CancelAll clears it).
    void Queue( LPDIRECT3DTEXTURE8* pOutTexture, const std::string& appId, ImageDownloadType type );

    // Cancel all pending downloads; in-flight download is aborted. Safe to queue a new batch after.
    void CancelAll();

    // Call from main thread (e.g. each frame). Applies completed downloads: loads file to texture and assigns *pOutTexture; releases old texture at pOut first.
    void ProcessCompleted( LPDIRECT3DDEVICE8 pd3dDevice );

private:
    struct Request
    {
        LPDIRECT3DTEXTURE8* pOutTexture;
        std::string appId;
        ImageDownloadType type;
    };
    struct Completed
    {
        LPDIRECT3DTEXTURE8* pOutTexture;
        std::string filePath;
    };

    static DWORD WINAPI ThreadProc( LPVOID param );
    void WorkerLoop();

    std::deque<Request>   m_queue;
    CRITICAL_SECTION       m_queueLock;
    std::deque<Completed> m_completed;
    CRITICAL_SECTION       m_completedLock;
    HANDLE                 m_wakeEvent;
    HANDLE                 m_thread;
    volatile bool          m_quit;
    volatile bool          m_cancelRequested;
    unsigned int           m_tempCounter;
};
