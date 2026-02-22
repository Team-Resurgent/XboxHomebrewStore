//=============================================================================
// ImageDownloader.cpp - Threaded queue for cover/screenshot image downloads
//=============================================================================

#include "ImageDownloader.h"
#include "Defines.h"
#include "WebManager.h"
#include "TextureHelper.h"
#include "Debug.h"
#include "String.h"
static uint32_t CRC32( const void* data, size_t size )
{
    static uint32_t s_table[256];
    static int32_t s_tableInit = 0;
    if( !s_tableInit )
    {
        for( uint32_t i = 0; i < 256; i++ )
        {
            uint32_t c = i;
            for( int32_t k = 0; k < 8; k++ )
                c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            s_table[i] = c;
        }
        s_tableInit = 1;
    }
    uint32_t crc = 0xFFFFFFFFU;
    const uint8_t* p = (const uint8_t*)data;
    for( size_t i = 0; i < size; i++ )
        crc = s_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

static std::string CachePathFor( const std::string& appId, ImageDownloadType type )
{
    uint32_t crc = CRC32( appId.c_str(), appId.size() );
    if( type == IMAGE_COVER ) {
        return String::Format( "T:\\Cache\\Covers\\%08X.jpg", crc );
    }
    return String::Format( "T:\\Cache\\Screenshots\\%08X.jpg", crc );
}

static bool FileExists( const char* path )
{
    DWORD att = GetFileAttributesA( path );
    if( att == (DWORD)-1 ) {
        return false;  /* path missing or error */
    }
    if( att & FILE_ATTRIBUTE_DIRECTORY ) {
        return false;
    }
    return true;  /* exists and is a file (not a directory) */
}

std::string ImageDownloader::GetCoverCachePath( const std::string& appId )
{
    return CachePathFor( appId, IMAGE_COVER );
}

bool ImageDownloader::IsCoverCached( const std::string& appId )
{
    return FileExists( GetCoverCachePath( appId ).c_str() );
}

#define CACHE_FILE_LIMIT STORE_CACHE_FILE_LIMIT

static void CollectFileWithTime( const char* dir, std::vector<std::pair<std::string, ULONGLONG> >* out )
{
    std::string pattern = std::string( dir ) + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA( pattern.c_str(), &fd );
    if( h == INVALID_HANDLE_VALUE ) {
        return;
    }
    do
    {
        if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
            continue;
        }
        ULARGE_INTEGER u;
        u.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        out->push_back( std::make_pair( std::string( dir ) + "\\" + fd.cFileName, u.QuadPart ) );
    } while( FindNextFileA( h, &fd ) );
    FindClose( h );
}

static int32_t CountCacheFiles()
{
    std::vector<std::pair<std::string, ULONGLONG> > files;
    CollectFileWithTime( "T:\\Cache\\Covers", &files );
    CollectFileWithTime( "T:\\Cache\\Screenshots", &files );
    return (int32_t)files.size();
}

static std::string FindOldestCacheFile()
{
    std::vector<std::pair<std::string, ULONGLONG> > files;
    CollectFileWithTime( "T:\\Cache\\Covers", &files );
    CollectFileWithTime( "T:\\Cache\\Screenshots", &files );
    if( files.empty() ) return std::string();
    size_t oldest = 0;
    for( size_t i = 1; i < files.size(); i++ ) {
        if( files[i].second < files[oldest].second ) {
            oldest = i;
        }
    }
    return files[oldest].first;
}

static void EnforceCacheLimit()
{
    while( CountCacheFiles() >= CACHE_FILE_LIMIT )
    {
        std::string old = FindOldestCacheFile();
        if( old.empty() ) {
            break;
        }
        DeleteFileA( old.c_str() );
    }
}

ImageDownloader::ImageDownloader()
    : m_thread( nullptr )
    , m_quit( false )
    , m_cancelRequested( false )
{
    InitializeCriticalSection( &m_queueLock );
    m_thread = CreateThread( nullptr, 0, ThreadProc, this, 0, nullptr );
}

ImageDownloader::~ImageDownloader()
{
    m_quit = true;
    if( m_thread )
    {
        WaitForSingleObject( m_thread, INFINITE );
        CloseHandle( m_thread );
    }
    DeleteCriticalSection( &m_completedLock );
    DeleteCriticalSection( &m_queueLock );
}

void ImageDownloader::Queue( D3DTexture** pOutTexture, const std::string& appId, ImageDownloadType type)
{
    if( !pOutTexture || appId.empty() ) return;

    for (uint32_t i = 0; i < m_queue.size(); i++) {
        Request* item = &m_queue[i];
        if (item->appId == appId && item->type == type) {
            return;
        }
    }

    Request r;
    r.pOutTexture = pOutTexture;
    r.appId = appId;
    r.type = type;

    EnterCriticalSection( &m_queueLock );
    m_queue.push_back( r );
    LeaveCriticalSection( &m_queueLock );
}

void ImageDownloader::CancelAll()
{
    m_cancelRequested = true;
    EnterCriticalSection( &m_queueLock );
    m_queue.clear();
    LeaveCriticalSection( &m_queueLock );
}

//void ImageDownloader::ProcessCompleted()
//{
//    std::deque<Completed> done;
//    EnterCriticalSection( &m_completedLock );
//    done.swap( m_completed );
//    LeaveCriticalSection( &m_completedLock );
//    for( size_t i = 0; i < done.size(); i++ )
//    {
//        Completed& c = done[i];
//        if( !c.pOutTexture ) continue;
//        if( *c.pOutTexture )
//        {
//            (*c.pOutTexture)->Release();
//            *c.pOutTexture = nullptr;
//        }
//        D3DTexture* pTex = TextureHelper::LoadFromFile( c.filePath );
//        if( pTex )
//        {
//            if( m_completionCallback )
//            {
//                if( !m_completionCallback( m_completionCtx, c.pOutTexture, c.filePath, c.appId, pTex ) )
//                    pTex->Release();
//                else
//                    *c.pOutTexture = pTex;
//            }
//            else
//                *c.pOutTexture = pTex;
//        }
//    }
//}

DWORD WINAPI ImageDownloader::ThreadProc( LPVOID param )
{
    ImageDownloader* p = (ImageDownloader*)param;
    if( p ) {
        p->WorkerLoop();
    }
    return 0;
}

void ImageDownloader::WorkerLoop()
{
    while( !m_quit )
    {
        if( m_quit ) {
            break;
        }

        Request req;
        bool moreInQueue = false;

        EnterCriticalSection( &m_queueLock );
        bool haveRequest = !m_queue.empty();
        if( haveRequest )
        {
            req = m_queue.front();
            m_queue.pop_front();
        }
        if ( m_queue.empty() )
            m_cancelRequested = false;
        LeaveCriticalSection( &m_queueLock );


        if (!haveRequest )
        {
            Sleep(10);
            continue;
        }

        std::string path = CachePathFor( req.appId, req.type );
        bool haveFile = FileExists( path.c_str() );
        if( !haveFile )
        {
            EnforceCacheLimit();
            bool ok = false;
            if( req.type == IMAGE_COVER ) {
                ok = WebManager::TryDownloadCover( req.appId, 144, 204, path, nullptr, nullptr, &m_cancelRequested );
            } else {
                ok = WebManager::TryDownloadScreenshot( req.appId, 640, 360, path, nullptr, nullptr, &m_cancelRequested );
            }
            if( m_cancelRequested || m_quit ) {
                continue;
            }
            if( !ok ) {
                continue;
            }
        }

        //D3DTexture* pTex = TextureHelper::LoadFromFile( path );
        //*req.pOutTexture = pTex;
    }
}
