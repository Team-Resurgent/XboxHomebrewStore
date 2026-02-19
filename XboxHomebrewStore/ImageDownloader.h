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
    // highPriority: if true, request is processed before non-priority (e.g. for items currently in view).
    void Queue( LPDIRECT3DTEXTURE8* pOutTexture, const std::string& appId, ImageDownloadType type, bool highPriority = false );

    // Cancel all pending downloads; in-flight download is aborted. Safe to queue a new batch after.
    void CancelAll();

    // Optional: set callback to decide whether to assign loaded texture. If set, ProcessCompleted calls it; if it returns true the texture is assigned to *pOut, else the texture is released.
    typedef bool (*CompletionCallback)( void* ctx, LPDIRECT3DTEXTURE8* pOut, const std::string& filePath, const std::string& appId, LPDIRECT3DTEXTURE8 loadedTex );
    void SetCompletionCallback( CompletionCallback fn, void* ctx ) { m_completionCallback = fn; m_completionCtx = ctx; }

    // Call from main thread (e.g. each frame). Applies completed downloads: loads file to texture and assigns *pOutTexture (or invokes callback); releases old texture at pOut first.
    void ProcessCompleted( LPDIRECT3DDEVICE8 pd3dDevice );

    // Cache lookup: path and existence for cover images (same path used when downloading).
    static std::string GetCoverCachePath( const std::string& appId );
    static bool IsCoverCached( const std::string& appId );

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
        std::string appId;
    };

    CompletionCallback m_completionCallback;
    void*              m_completionCtx;

    static DWORD WINAPI ThreadProc( LPVOID param );
    void WorkerLoop();

    std::deque<Request>   m_queue;
    std::deque<Request>   m_queuePriority;
    CRITICAL_SECTION       m_queueLock;
    std::deque<Completed> m_completed;
    CRITICAL_SECTION       m_completedLock;
    HANDLE                 m_wakeEvent;
    HANDLE                 m_thread;
    volatile bool          m_quit;
    volatile bool          m_cancelRequested;
    unsigned int           m_tempCounter;
};
