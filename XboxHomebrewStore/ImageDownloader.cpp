//=============================================================================
// ImageDownloader.cpp - Threaded download + background JPG->DXT1 conversion
//
// Pipeline:
//   DownloadThread  -- HTTP fetch, saves .jpg to disk
//   ConverterThread -- D3DXCreateTextureFromFileEx(.jpg) -> WriteDxt1DDS(.dxt)
//                      -> Release tex -> DeleteFile(.jpg)
//   Main thread     -- LoadDxtFromFile(.dxt): CreateTexture+LockRect+memcpy+UnlockRect
//=============================================================================

#include "ImageDownloader.h"
#include "Context.h"
#include "StoreList.h"
#include "StoreManager.h"
#include "FileSystem.h"
#include "Defines.h"
#include "WebManager.h"
#include "Debug.h"
#include "String.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static uint32_t CRC32( const void* data, size_t size )
{
    static uint32_t s_table[256];
    static int32_t  s_tableInit = 0;
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

static std::string CachePathFor( const std::string appId, ImageDownloadType type )
{
    uint32_t crc = CRC32( appId.c_str(), appId.size() );
    std::string root = StoreList::GetActiveCacheRoot();
    if( type == IMAGE_COVER )
        return String::Format( "%s\\Covers\\%08X.jpg", root.c_str(), crc );
    return String::Format( "%s\\Screenshots\\%08X.jpg", root.c_str(), crc );
}

static bool FileExists( const char* path )
{
    DWORD att = GetFileAttributesA( path );
    if( att == (DWORD)-1 )            return false;
    if( att & FILE_ATTRIBUTE_DIRECTORY ) return false;
    return true;
}

static bool FileExistsAndAvailable( const char* path )
{
    DWORD att = GetFileAttributesA( path );
    if( att == (DWORD)-1 )            return false;
    if( att & FILE_ATTRIBUTE_DIRECTORY ) return false;
    if( att & FILE_ATTRIBUTE_ARCHIVE  ) return false; // still downloading
    return true;
}

// ---------------------------------------------------------------------------
// Stable cover count -- incremented only when a .dxt is fully written.
// Avoids per-frame directory scans that race with the converter thread.
static volatile LONG s_cachedCoverCount = 0;

// WriteDxt1DDS -- called from converter thread. Locks the DXT1 texture,
// writes a minimal DDS file, then unlocks. No GPU commands issued.
// Sets FILE_ATTRIBUTE_ARCHIVE before writing and clears it on success so
// FileExistsAndAvailable() rejects the file while it's being written.
// ---------------------------------------------------------------------------
static bool WriteDxt1DDS( const std::string& path, D3DTexture* tex )
{
    D3DSURFACE_DESC desc;
    if( FAILED( tex->GetLevelDesc( 0, &desc ) ) ) return false;

    D3DLOCKED_RECT locked;
    if( FAILED( tex->LockRect( 0, &locked, NULL, D3DLOCK_READONLY ) ) ) return false;

    UINT bw       = (desc.Width  + 3) / 4;
    UINT bh       = (desc.Height + 3) / 4;
    UINT dataSize = bw * bh * 8;

    // Mark as in-progress BEFORE creating the file so FileExistsAndAvailable
    // never sees a zero-byte file with NORMAL attributes (bad DDS magic race).
    // Touch the file first so SetFileAttributesA has something to set.
    {
        FILE* touch = fopen( path.c_str(), "wb" );
        if( touch ) fclose( touch );
    }
    SetFileAttributesA( path.c_str(), FILE_ATTRIBUTE_ARCHIVE );

    FILE* f = fopen( path.c_str(), "wb" );
    if( f == NULL ) { tex->UnlockRect( 0 ); DeleteFileA( path.c_str() ); return false; }

    DWORD magic = 0x20534444; // 'DDS '
    fwrite( &magic, 4, 1, f );

    DWORD hdr[31];
    memset( hdr, 0, sizeof(hdr) );
    hdr[0]  = 124;
    hdr[1]  = 0x00081007;  // CAPS|HEIGHT|WIDTH|PIXELFORMAT|LINEARSIZE
    hdr[2]  = desc.Height;
    hdr[3]  = desc.Width;
    hdr[4]  = dataSize;
    hdr[6]  = 1;           // mip count
    hdr[18] = 32;          // ddpfPixelFormat.dwSize
    hdr[19] = 0x00000004;  // DDPF_FOURCC
    hdr[20] = 0x31545844;  // 'DXT1'
    hdr[26] = 0x00001000;  // DDSCAPS_TEXTURE
    fwrite( hdr, sizeof(hdr), 1, f );
    fwrite( locked.pBits, dataSize, 1, f );
    fflush( f );              // flush CRT buffer to OS
    _commit( _fileno( f ) );  // flush OS write cache to disk before we mark NORMAL
    fclose( f );
    tex->UnlockRect( 0 );

    // Clear ARCHIVE -- file is fully written, main thread can now load it
    SetFileAttributesA( path.c_str(), FILE_ATTRIBUTE_NORMAL );

    // Only increment cover count -- not screenshots
    if( path.find( "\\Covers\\" ) != std::string::npos ) {
        InterlockedIncrement( (LPLONG)&s_cachedCoverCount );
    }

    Debug::Print( "WriteDxt1DDS: wrote %s (%dx%d %d bytes)\n",
        path.c_str(), desc.Width, desc.Height, dataSize );
    return true;
}

// ---------------------------------------------------------------------------
// Cache helpers
// ---------------------------------------------------------------------------

static void CollectFileWithTime( const char* dir,
    std::vector<std::pair<std::string,ULONGLONG> >* out )
{
    std::string pattern = std::string(dir) + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA( pattern.c_str(), &fd );
    if( h == INVALID_HANDLE_VALUE ) return;
    do {
        if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;
        ULARGE_INTEGER u;
        u.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
        u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        out->push_back( std::make_pair(
            std::string(dir) + "\\" + fd.cFileName, u.QuadPart ) );
    } while( FindNextFileA( h, &fd ) );
    FindClose( h );
}

static int32_t CountCacheFiles()
{
    std::string root = StoreList::GetActiveCacheRoot();
    std::vector<std::pair<std::string,ULONGLONG> > files;
    CollectFileWithTime( (root + "\\Covers").c_str(),      &files );
    CollectFileWithTime( (root + "\\Screenshots").c_str(), &files );
    return (int32_t)files.size();
}

static std::string FindOldestCacheFile()
{
    std::string root = StoreList::GetActiveCacheRoot();
    std::vector<std::pair<std::string,ULONGLONG> > files;
    CollectFileWithTime( (root + "\\Covers").c_str(),      &files );
    CollectFileWithTime( (root + "\\Screenshots").c_str(), &files );
    if( files.empty() ) return std::string();
    size_t oldest = 0;
    for( size_t i = 1; i < files.size(); i++ )
        if( files[i].second < files[oldest].second ) oldest = i;
    return files[oldest].first;
}

static void EnforceCacheLimit()
{
    while( CountCacheFiles() >= STORE_CACHE_FILE_LIMIT )
    {
        std::string old = FindOldestCacheFile();
        if( old.empty() ) break;
        DeleteFileA( old.c_str() );
    }
}

void ImageDownloader::ResetCachedCoverCount()
{
    // Do a one-time scan at startup (before converter thread runs)
    std::string root = StoreList::GetActiveCacheRoot();
    std::vector<std::pair<std::string,ULONGLONG> > files;
    CollectFileWithTime( (root + "\\Covers").c_str(), &files );
    LONG count = 0;
    for( uint32_t i = 0; i < files.size(); i++ ) {
        const std::string& f = files[i].first;
        if( f.size() >= 4 && f.substr( f.size() - 4 ) == ".dxt" ) { count++; }
    }
    InterlockedExchange( (LPLONG)&s_cachedCoverCount, count );
}

int32_t ImageDownloader::GetCachedCoverCount()
{
    return (int32_t)s_cachedCoverCount;
}

static std::string FailPathFor( const std::string& cachedPath ) {
    size_t slash = cachedPath.rfind( '\\' );
    std::string dir  = cachedPath.substr( 0, slash + 1 ) + "Failed";
    std::string name = cachedPath.substr( slash + 1 );
    size_t dot = name.rfind( '.' );
    if( dot != std::string::npos ) name = name.substr( 0, dot );
    return dir + "\\" + name + ".fail";
}

bool ImageDownloader::IsCoverFailed( const std::string appId ) {
    return FileExists( FailPathFor( GetCoverCachePath( appId ) ).c_str() );
}
void ImageDownloader::ClearFailedCover( const std::string appId ) {
    DeleteFileA( FailPathFor( GetCoverCachePath( appId ) ).c_str() );
}
bool ImageDownloader::IsScreenshotFailed( const std::string appId ) {
    return FileExists( FailPathFor( GetScreenshotCachePath( appId ) ).c_str() );
}
void ImageDownloader::ClearFailedScreenshot( const std::string appId ) {
    DeleteFileA( FailPathFor( GetScreenshotCachePath( appId ) ).c_str() );
}
int32_t ImageDownloader::GetFailedCoverCount() {
    std::string root = StoreList::GetActiveCacheRoot();
    std::vector<std::pair<std::string,ULONGLONG> > files;
    CollectFileWithTime( (root + "\\Covers\\Failed").c_str(),      &files );
    CollectFileWithTime( (root + "\\Screenshots\\Failed").c_str(), &files );
    return (int32_t)files.size();
}
void ImageDownloader::ClearFailedCovers() {
    std::string root = StoreList::GetActiveCacheRoot();
    std::string cf = root + "\\Covers\\Failed";
    std::string sf = root + "\\Screenshots\\Failed";
    FileSystem::DirectoryDelete( cf.c_str(), true );
    FileSystem::DirectoryDelete( sf.c_str(), true );
    FileSystem::DirectoryCreate( cf.c_str() );
    FileSystem::DirectoryCreate( sf.c_str() );
    StoreManager::InvalidateCovers();
    Debug::Print( "ImageDownloader: cleared failed markers\n" );
}

// ---------------------------------------------------------------------------
// Public path helpers
// ---------------------------------------------------------------------------

std::string ImageDownloader::GetCoverCachePath( const std::string appId )
{
    std::string root = StoreList::GetActiveCacheRoot();
    uint32_t crc = CRC32( appId.c_str(), appId.size() );
    std::string dxtPath = String::Format( "%s\\Covers\\%08X.dxt", root.c_str(), crc );
    if( FileExistsAndAvailable( dxtPath.c_str() ) ) return dxtPath;
    return String::Format( "%s\\Covers\\%08X.jpg", root.c_str(), crc );
}

std::string ImageDownloader::GetCoverJpgPath( const std::string appId )
{
    std::string root = StoreList::GetActiveCacheRoot();
    uint32_t crc = CRC32( appId.c_str(), appId.size() );
    return String::Format( "%s\\Covers\\%08X.jpg", root.c_str(), crc );
}

bool ImageDownloader::IsCoverCached( const std::string appId )
{
    std::string root = StoreList::GetActiveCacheRoot();
    uint32_t crc = CRC32( appId.c_str(), appId.size() );
    std::string dxtPath = String::Format( "%s\\Covers\\%08X.dxt", root.c_str(), crc );
    return FileExistsAndAvailable( dxtPath.c_str() );
}

std::string ImageDownloader::GetScreenshotCachePath( const std::string appId )
{
    std::string root = StoreList::GetActiveCacheRoot();
    uint32_t crc = CRC32( appId.c_str(), appId.size() );
    std::string dxtPath = String::Format( "%s\\Screenshots\\%08X.dxt", root.c_str(), crc );
    if( FileExistsAndAvailable( dxtPath.c_str() ) ) return dxtPath;
    return String::Format( "%s\\Screenshots\\%08X.jpg", root.c_str(), crc );
}

bool ImageDownloader::IsScreenshotCached( const std::string appId )
{
    return FileExistsAndAvailable( GetScreenshotCachePath( appId ).c_str() );
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ImageDownloader::ImageDownloader()
    : m_downloadThread( NULL )
    , m_convertThread( NULL )
    , m_quit( false )
    , m_cancelRequested( false )
    , m_busy( false )
    , m_busyType( IMAGE_COVER )
    , m_convertBusy( false )
{
    InitializeCriticalSection( &m_queueLock );
    InitializeCriticalSection( &m_convertLock );

    m_downloadThread = CreateThread( NULL, 0, DownloadThreadProc, this, 0, NULL );
    m_convertThread  = CreateThread( NULL, 0, ConverterThreadProc, this, 0, NULL );
}

ImageDownloader::~ImageDownloader()
{
    m_quit = true;
    if( m_downloadThread )
    {
        WaitForSingleObject( m_downloadThread, INFINITE );
        CloseHandle( m_downloadThread );
    }
    if( m_convertThread )
    {
        WaitForSingleObject( m_convertThread, INFINITE );
        CloseHandle( m_convertThread );
    }
    DeleteCriticalSection( &m_queueLock );
    DeleteCriticalSection( &m_convertLock );
}

// ---------------------------------------------------------------------------
// Queue / WarmCache / CancelAll / FlushQueue
// ---------------------------------------------------------------------------

void ImageDownloader::Queue( D3DTexture** pOutTexture, const std::string appId,
    ImageDownloadType type )
{
    if( !pOutTexture || appId.empty() ) return;

    EnterCriticalSection( &m_queueLock );

    // Dedup against in-flight download
    if( m_busy && m_busyAppId == appId && m_busyType == type )
    {
        LeaveCriticalSection( &m_queueLock );
        return;
    }

    // Dedup against pending queue
    for( uint32_t i = 0; i < m_queue.size(); i++ )
    {
        if( m_queue[i].appId == appId && m_queue[i].type == type )
        {
            LeaveCriticalSection( &m_queueLock );
            return;
        }
    }
    Request r;
    r.pOutTexture = pOutTexture;
    r.appId = appId;
    r.type  = type;
    m_queue.push_back( r );
    LeaveCriticalSection( &m_queueLock );
}

void ImageDownloader::WarmCache( const std::string appId, ImageDownloadType type )
{
    if( appId.empty() ) return;
    if( type == IMAGE_COVER      && IsCoverCached( appId )      ) return;
    if( type == IMAGE_SCREENSHOT && IsScreenshotCached( appId ) ) return;

    EnterCriticalSection( &m_queueLock );

    // Dedup against in-flight download
    if( m_busy && m_busyAppId == appId && m_busyType == type )
    {
        LeaveCriticalSection( &m_queueLock );
        return;
    }

    for( uint32_t i = 0; i < m_queue.size(); i++ )
    {
        if( m_queue[i].appId == appId && m_queue[i].type == type )
        {
            LeaveCriticalSection( &m_queueLock );
            return;
        }
    }
    Request r;
    r.pOutTexture = NULL;
    r.appId = appId;
    r.type  = type;
    m_queue.push_back( r );
    LeaveCriticalSection( &m_queueLock );
}

void ImageDownloader::CancelAll()
{
    m_cancelRequested = true;

    EnterCriticalSection( &m_queueLock );
    m_queue.clear();
    LeaveCriticalSection( &m_queueLock );

    // Also drain the converter queue -- don't convert covers we're discarding
    EnterCriticalSection( &m_convertLock );
    m_convertQueue.clear();
    LeaveCriticalSection( &m_convertLock );

    // Clear failed set -- cancelled items should be retryable when visible again
    m_failed.clear();
}

void ImageDownloader::FlushQueue()
{
    EnterCriticalSection( &m_queueLock );
    m_queue.clear();
    LeaveCriticalSection( &m_queueLock );
}

// ---------------------------------------------------------------------------
// SetVisibleQueue -- replaces the pending queue with exactly the currently
// visible items that still need covers. If the in-flight download is for
// something no longer visible, cancels it so new items start immediately.
// The download thread clears m_cancelRequested when it picks up the next item.
// ---------------------------------------------------------------------------
void ImageDownloader::SetVisibleQueue( D3DTexture** pOutTextures[],
    const std::string appIds[], int32_t count )
{
    EnterCriticalSection( &m_queueLock );

    m_queue.clear();

    // Check if the in-flight download is still needed
    bool inFlightStillNeeded = false;
    if( m_busy && !m_busyAppId.empty() )
    {
        for( int32_t i = 0; i < count; i++ )
        {
            if( appIds[i] == m_busyAppId ) { inFlightStillNeeded = true; break; }
        }
    }

    // Cancel in-flight download if it's for an off-screen item
    if( m_busy && !inFlightStillNeeded )
        m_cancelRequested = true;

    for( int32_t i = 0; i < count; i++ )
    {
        if( appIds[i].empty() ) continue;

        // Skip if already downloading this exact item
        if( m_busy && m_busyAppId == appIds[i] && m_busyType == IMAGE_COVER )
            continue;

        Request r;
        r.pOutTexture = pOutTextures[i];
        r.appId       = appIds[i];
        r.type        = IMAGE_COVER;
        m_queue.push_back( r );
    }

    LeaveCriticalSection( &m_queueLock );
}

bool ImageDownloader::HasPendingWork() const
{
    EnterCriticalSection( const_cast<CRITICAL_SECTION*>(&m_queueLock) );
    bool pending = !m_queue.empty();
    LeaveCriticalSection( const_cast<CRITICAL_SECTION*>(&m_queueLock) );

    if( pending || m_busy ) return true;

    // Also consider convert queue
    EnterCriticalSection( const_cast<CRITICAL_SECTION*>(&m_convertLock) );
    bool converting = !m_convertQueue.empty();
    LeaveCriticalSection( const_cast<CRITICAL_SECTION*>(&m_convertLock) );

    return converting || m_convertBusy;
}

// ---------------------------------------------------------------------------
// Download thread
// ---------------------------------------------------------------------------

DWORD WINAPI ImageDownloader::DownloadThreadProc( LPVOID param )
{
    ImageDownloader* p = (ImageDownloader*)param;
    if( p ) p->DownloadLoop();
    return 0;
}

void ImageDownloader::DownloadLoop()
{
    while( !m_quit )
    {
        Request req;
        bool haveRequest = false;

        EnterCriticalSection( &m_queueLock );
        if( !m_queue.empty() )
        {
            req = m_queue.front();
            m_queue.pop_front();
            haveRequest = true;
            m_cancelRequested = false; // new item -- clear any stale cancel
        }
        LeaveCriticalSection( &m_queueLock );

        if( !haveRequest ) { Sleep(10); continue; }

        m_busy = true;

        std::string jpgPath = CachePathFor( req.appId, req.type );

        // Record in-flight appId so Queue() can dedup against it
        EnterCriticalSection( &m_queueLock );
        m_busyAppId = req.appId;
        m_busyType  = req.type;
        LeaveCriticalSection( &m_queueLock );

        std::string failKey = req.appId;
        failKey += (req.type == IMAGE_COVER) ? "_cover" : "_screenshot";

        // If .dxt already exists this cover is fully ready -- skip entirely
        if( req.type == IMAGE_COVER ) {
            std::string dxtCheck = jpgPath.substr( 0, jpgPath.size() - 4 ) + ".dxt";
            if( FileExistsAndAvailable( dxtCheck.c_str() ) ) {
                EnterCriticalSection( &m_queueLock );
                m_busyAppId = "";
                LeaveCriticalSection( &m_queueLock );
                m_busy = false;
                continue;
            }
        }
        bool haveFile        = FileExists( jpgPath.c_str() );
        bool previouslyFailed = ( m_failed.find( failKey ) != m_failed.end() );

        if( !haveFile && !previouslyFailed )
        {
            EnforceCacheLimit();

            bool ok = false;
#ifdef USE_SERVER_DDS
            // Server DDS mode: download pre-converted DDS, save as .dxt,
            // skip converter thread entirely.
            std::string dxtPath = jpgPath.substr( 0, jpgPath.size() - 4 ) + ".dxt";
            if( req.type == IMAGE_COVER )
            {
                ok = WebManager::TryDownloadCoverDds(
                    req.appId, 288, 408, dxtPath, NULL, NULL, &m_cancelRequested );
            }
            else
            {
                ok = WebManager::TryDownloadScreenshotDds(
                    req.appId, 640, 360, dxtPath, NULL, NULL, &m_cancelRequested );
            }
            if( ok ) {
                // Mark as available and count -- no converter needed
                SetFileAttributesA( dxtPath.c_str(), FILE_ATTRIBUTE_NORMAL );
                if( req.type == IMAGE_COVER ) {
                    InterlockedIncrement( (LPLONG)&s_cachedCoverCount );
                }
                Debug::Print( "ImageDownloader: server DDS saved %s\n", dxtPath.c_str() );
            } else {
                DeleteFileA( dxtPath.c_str() );
                std::string fp = FailPathFor( dxtPath );
                FILE *ff = fopen( fp.c_str(), "wb" ); if( ff ) fclose( ff );
                m_failed.insert( failKey );
            }
            EnterCriticalSection( &m_queueLock );
            m_busyAppId = "";
            LeaveCriticalSection( &m_queueLock );
            m_busy = false;
            continue;
#else
            if( req.type == IMAGE_COVER )
            {
                ok = WebManager::TryDownloadCover(
                    req.appId, 144, 204, jpgPath, NULL, NULL, &m_cancelRequested );
            }
            else
            {
                ok = WebManager::TryDownloadScreenshot(
                    req.appId, 640, 360, jpgPath, NULL, NULL, &m_cancelRequested );
            }
#endif

            if( m_cancelRequested || m_quit )
            {
                DeleteFileA( jpgPath.c_str() ); // remove partial file
                EnterCriticalSection( &m_queueLock );
                m_busyAppId = "";
                LeaveCriticalSection( &m_queueLock );
                m_busy = false;
                continue;
            }

            if( !ok )
            {
                DeleteFileA( jpgPath.c_str() ); // remove partial/corrupt file
                {
                    std::string fp = FailPathFor( jpgPath );
                    FILE *ff = fopen( fp.c_str(), "wb" );
                    if( ff ) fclose( ff );
                }
                m_failed.insert( failKey );
                EnterCriticalSection( &m_queueLock );
                m_busyAppId = "";
                LeaveCriticalSection( &m_queueLock );
                m_busy = false;
                continue;
            }
        }

        // Download succeeded (or file already existed) -- push to converter queue.
        // Converter thread will do D3DX decode + DXT1 compress + write .dxt + delete .jpg.
        // Queue both covers and screenshots for DXT conversion.
        {
            EnterCriticalSection( &m_convertLock );
            // Avoid duplicates
            bool alreadyQueued = false;
            for( uint32_t i = 0; i < m_convertQueue.size(); i++ )
            {
                if( m_convertQueue[i] == jpgPath ) { alreadyQueued = true; break; }
            }
            if( !alreadyQueued ) m_convertQueue.push_back( jpgPath );
            LeaveCriticalSection( &m_convertLock );
        }

        EnterCriticalSection( &m_queueLock );
        m_busyAppId = "";
        LeaveCriticalSection( &m_queueLock );
        m_busy = false;
    }
}

// ---------------------------------------------------------------------------
// Converter thread -- D3DX JPEG decode + DXT1 compress, off main thread
// ---------------------------------------------------------------------------

DWORD WINAPI ImageDownloader::ConverterThreadProc( LPVOID param )
{
    ImageDownloader* p = (ImageDownloader*)param;
    if( p ) p->ConverterLoop();
    return 0;
}

void ImageDownloader::ConverterLoop()
{
    while( !m_quit )
    {
        std::string jpgPath;
        bool haveWork = false;

        EnterCriticalSection( &m_convertLock );
        if( !m_convertQueue.empty() )
        {
            jpgPath  = m_convertQueue.front();
            m_convertQueue.pop_front();
            haveWork = true;
        }
        LeaveCriticalSection( &m_convertLock );

        if( !haveWork ) { Sleep(10); continue; }

        // Skip if .jpg is still downloading (ARCHIVE flag set)
        if( !FileExistsAndAvailable( jpgPath.c_str() ) )
        {
            // Put it back at the end and try again later
            EnterCriticalSection( &m_convertLock );
            m_convertQueue.push_back( jpgPath );
            LeaveCriticalSection( &m_convertLock );
            Sleep(50);
            continue;
        }

        m_convertBusy = true;

        std::string dxtPath = jpgPath.substr( 0, jpgPath.size() - 4 ) + ".dxt";

        // D3DXCreateTextureFromFileEx is safe to call from a non-main thread
        // on original Xbox -- it does CPU work only (JPEG decode + DXT compress).
        D3DTexture* tex = NULL;
        HRESULT hr = D3DXCreateTextureFromFileEx(
            Context::GetD3dDevice(),
            jpgPath.c_str(),
            D3DX_DEFAULT, D3DX_DEFAULT, 1,
            0, D3DFMT_DXT1, D3DPOOL_DEFAULT,
            D3DX_DEFAULT, D3DX_DEFAULT,
            0, NULL, NULL, &tex );

        if( FAILED(hr) || tex == NULL )
        {
            Debug::Print( "ConverterLoop: D3DX failed on %s hr=0x%08X\n",
                jpgPath.c_str(), (unsigned int)hr );
            DeleteFileA( jpgPath.c_str() ); // corrupt jpg -- delete so it re-downloads
            m_convertBusy = false;
            continue;
        }

        if( WriteDxt1DDS( dxtPath, tex ) )
        {
            DeleteFileA( jpgPath.c_str() );
            Debug::Print( "ConverterLoop: converted %s\n", dxtPath.c_str() );
        }
        else
        {
            Debug::Print( "ConverterLoop: WriteDxt1DDS failed %s\n", dxtPath.c_str() );
            DeleteFileA( jpgPath.c_str() ); // delete jpg so it re-downloads
            DeleteFileA( dxtPath.c_str() ); // delete partial/0KB dxt so LoadFromFile doesn't choke on it
        }

        tex->Release();
        m_convertBusy = false;
    }
}