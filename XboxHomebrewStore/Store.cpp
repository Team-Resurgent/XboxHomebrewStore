//=============================================================================
// Store.cpp - Xbox Homebrew Store Implementation
//=============================================================================

#include "Main.h"
#include "Store.h"
#include "Defines.h"
#include "WebManager.h"
#include "String.h"
#include <stdlib.h>
#include <string.h>

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
Store::Store()
{
    m_pItems = NULL;
    m_nItemCount = 0;
    m_nFilteredCount = 0;
    m_pd3dDevice = NULL;
    m_pFilteredIndices = NULL;
    m_visibleStart = 0;
    m_visibleCount = 0;
    m_pNextItems = NULL;
    m_nNextItemCount = 0;
    m_nChunkApiPage = 1;
    m_nDisplayPage = 0;
    m_downloadNow = 0;
    m_downloadTotal = 0;
    m_downloadDone = false;
    m_downloadSuccess = false;
    m_downloadCancelRequested = false;
    m_downloadThread = NULL;

    CategoryItem allApps;
    allApps.category = "All Apps";
    allApps.count = 0;
    m_aCategories.push_back( allApps );

    m_nTotalPages = 1;
    m_nTotalCount = 0;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
Store::~Store()
{
    if ( m_pFilteredIndices )
    {
        delete[] m_pFilteredIndices;
        m_pFilteredIndices = NULL;
    }
    FreeNextChunk();
    if( m_pItems )
    {
        for( int i = 0; i < m_nItemCount; i++ )
        {
            if( m_pItems[i].pIcon ) m_pItems[i].pIcon->Release();
            if( m_pItems[i].pScreenshot ) m_pItems[i].pScreenshot->Release();
        }
        delete[] m_pItems;
        m_pItems = NULL;
    }
}

//-----------------------------------------------------------------------------
// BuildCategoryList - Count apps in each category
//-----------------------------------------------------------------------------
void Store::BuildCategoryList()
{
    for( size_t i = 0; i < m_aCategories.size(); i++ ) {
        m_aCategories[i].count = 0;
    }
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( m_pItems[i].nCategoryIndex >= 0 && m_pItems[i].nCategoryIndex < (int)m_aCategories.size() )
        {
            m_aCategories[m_pItems[i].nCategoryIndex].count++;
            m_aCategories[0].count++;
        }
    }
    OutputDebugString( String::Format( "Categories: %d total\n", (int)m_aCategories.size() ).c_str() );
    for( size_t i = 0; i < m_aCategories.size(); i++ )
    {
        OutputDebugString( String::Format( "  %s: %u apps\n", m_aCategories[i].category.c_str(), (unsigned)m_aCategories[i].count ).c_str() );
    }
}

//-----------------------------------------------------------------------------
// FindCategoryIndex - Look up category index by ID (for web-loaded apps)
//-----------------------------------------------------------------------------
int Store::FindCategoryIndex( const char* catID ) const
{
    if( !catID || !catID[0] ) { return 0; }
    std::string key( catID );
    for( size_t i = 0; i < m_aCategories.size(); i++ )
    {
        if( m_aCategories[i].category == key ) {
            return (int)i;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
// LoadCategoriesFromWeb - Fetch categories from API
//-----------------------------------------------------------------------------
BOOL Store::LoadCategoriesFromWeb()
{
    CategoriesResponse resp;
    if( !WebManager::TryGetCategories( resp ) )
    {
        OutputDebugString( "Failed to load categories from web\n" );
        return FALSE;
    }
    m_aCategories.clear();
    CategoryItem allApps;
    allApps.category = "All Apps";
    allApps.count = 0;
    m_aCategories.push_back( allApps );
    for( size_t i = 0; i < resp.size(); i++ )
    {
        const CategoryItem& c = resp[i];
        CategoryItem item;
        item.category = c.category;
        if( !item.category.empty() && item.category[0] >= 'a' && item.category[0] <= 'z' ) {
            item.category[0] -= 32;
        }
        item.count = c.count;
        m_aCategories.push_back( item );
    }
    OutputDebugString( "Categories loaded from web\n" );
    return TRUE;
}

//-----------------------------------------------------------------------------
// LoadAppsPage - Fetch one chunk (cols*rows*3) of apps
//-----------------------------------------------------------------------------
BOOL Store::LoadAppsPage( int page, const char* categoryFilter, int selectedCategoryForCount )
{
    m_currentCategoryFilter = categoryFilter ? categoryFilter : "";
    FreeNextChunk();
    AppsResponse resp;
    if( !WebManager::TryGetApps( resp, (uint32_t)page, (uint32_t)STORE_CHUNK_SIZE, m_currentCategoryFilter, "" ) )
    {
        OutputDebugString( "Failed to load apps page from web\n" );
        return FALSE;
    }
    if( resp.items.empty() && page == 1 )
    {
        m_nItemCount = 0;
        m_nFilteredCount = 0;
        m_nChunkApiPage = 1;
        m_nDisplayPage = 0;
        m_nTotalPages = 1;
        m_nTotalCount = 0;
        if( selectedCategoryForCount >= 0 && selectedCategoryForCount < (int)m_aCategories.size() ) {
            m_aCategories[selectedCategoryForCount].count = 0;
        }
        return TRUE;
    }
    m_imageDownloader.CancelAll();
    if( m_pItems )
    {
        for( int i = 0; i < m_nItemCount; i++ )
        {
            if( m_pItems[i].pIcon )
            {
                m_pItems[i].pIcon->Release();
                m_pItems[i].pIcon = NULL;
            }
            if( m_pItems[i].pScreenshot )
            {
                m_pItems[i].pScreenshot->Release();
                m_pItems[i].pScreenshot = NULL;
            }
        }
        delete[] m_pItems;
        m_pItems = NULL;
    }
    if( m_pFilteredIndices )
    {
        delete[] m_pFilteredIndices;
        m_pFilteredIndices = NULL;
    }
    m_nItemCount = (int)resp.items.size();
    m_pItems = new StoreItem[m_nItemCount];
    if( !m_pItems )
    {
        m_nItemCount = 0;
        return FALSE;
    }
    m_pFilteredIndices = new int[m_nItemCount];
    if( !m_pFilteredIndices )
    {
        delete[] m_pItems;
        m_pItems = NULL;
        m_nItemCount = 0;
        return FALSE;
    }
    for( int i = 0; i < m_nItemCount; i++ )
    {
        StoreItem* item = &m_pItems[i];
        item->app = resp.items[i];
        item->nCategoryIndex = FindCategoryIndex( item->app.category.c_str() );
        item->pIcon = NULL;
        item->pScreenshot = NULL;
        item->nSelectedVersion = 0;
        item->nVersionScrollOffset = 0;
        item->bViewingVersionDetail = FALSE;
        item->versions.clear();
        m_pFilteredIndices[i] = i;
    }
    m_nFilteredCount = m_nItemCount;
    m_nChunkApiPage = (int)resp.page;
    m_nDisplayPage = 0;
    m_nTotalCount = (int)resp.totalCount;
    m_nTotalPages = ( m_nTotalCount > 0 && STORE_ITEMS_PER_SCREEN > 0 ) ? ( ( m_nTotalCount + STORE_ITEMS_PER_SCREEN - 1 ) / STORE_ITEMS_PER_SCREEN ) : 1;
    if( selectedCategoryForCount >= 0 && selectedCategoryForCount < (int)m_aCategories.size() ) {
        m_aCategories[selectedCategoryForCount].count = (uint32_t)m_nTotalCount;
    }
    m_userState.Load( STORE_USER_STATE_PATH );
    m_userState.ApplyToStore( m_pItems, m_nItemCount );
    int visibleCount = ( m_nFilteredCount < STORE_MAX_ICONS_IN_RAM ) ? m_nFilteredCount : STORE_MAX_ICONS_IN_RAM;
    SetVisibleRange( 0, visibleCount );
    OutputDebugString( String::Format( "Loaded chunk page %d: %d apps (total %d)\n", m_nChunkApiPage, m_nItemCount, m_nTotalCount ).c_str() );
    return TRUE;
}

//-----------------------------------------------------------------------------
// Rolling window: next-chunk buffer and navigation
//-----------------------------------------------------------------------------
void Store::FreeNextChunk()
{
    if( !m_pNextItems ) return;
    for( int i = 0; i < m_nNextItemCount; i++ )
    {
        if( m_pNextItems[i].pIcon ) m_pNextItems[i].pIcon->Release();
        if( m_pNextItems[i].pScreenshot ) m_pNextItems[i].pScreenshot->Release();
    }
    delete[] m_pNextItems;
    m_pNextItems = NULL;
    m_nNextItemCount = 0;
}

int Store::GetTotalPages() const
{
    if( m_nTotalCount <= 0 || STORE_ITEMS_PER_SCREEN <= 0 ) return 1;
    return ( m_nTotalCount + STORE_ITEMS_PER_SCREEN - 1 ) / STORE_ITEMS_PER_SCREEN;
}

void Store::PreloadNextChunk()
{
    if( m_pNextItems ) return;
    AppsResponse resp;
    if( !WebManager::TryGetApps( resp, (uint32_t)( m_nChunkApiPage + 1 ), (uint32_t)STORE_CHUNK_SIZE, m_currentCategoryFilter, "" ) )
        return;
    if( resp.items.empty() ) return;
    int n = (int)resp.items.size();
    m_pNextItems = new StoreItem[n];
    if( !m_pNextItems ) return;
    m_nNextItemCount = n;
    for( int i = 0; i < n; i++ )
    {
        StoreItem* item = &m_pNextItems[i];
        item->app = resp.items[i];
        item->nCategoryIndex = FindCategoryIndex( item->app.category.c_str() );
        item->pIcon = NULL;
        item->pScreenshot = NULL;
        item->nSelectedVersion = 0;
        item->nVersionScrollOffset = 0;
        item->bViewingVersionDetail = FALSE;
        item->versions.clear();
    }
    m_userState.Load( STORE_USER_STATE_PATH );
    m_userState.ApplyToStore( m_pNextItems, m_nNextItemCount );
    OutputDebugString( String::Format( "Preloaded next chunk: %d apps\n", n ).c_str() );
}

bool Store::GoToNextChunk()
{
    if( !m_pNextItems || m_nNextItemCount <= 0 ) return false;
    m_imageDownloader.CancelAll();
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( m_pItems[i].pIcon ) { m_pItems[i].pIcon->Release(); m_pItems[i].pIcon = NULL; }
        if( m_pItems[i].pScreenshot ) { m_pItems[i].pScreenshot->Release(); m_pItems[i].pScreenshot = NULL; }
    }
    delete[] m_pItems;
    if( m_pFilteredIndices ) { delete[] m_pFilteredIndices; m_pFilteredIndices = NULL; }
    m_pItems = m_pNextItems;
    m_nItemCount = m_nNextItemCount;
    m_pNextItems = NULL;
    m_nNextItemCount = 0;
    m_nChunkApiPage++;
    m_nDisplayPage = 0;
    m_pFilteredIndices = new int[m_nItemCount];
    if( m_pFilteredIndices )
    {
        for( int i = 0; i < m_nItemCount; i++ ) m_pFilteredIndices[i] = i;
        m_nFilteredCount = m_nItemCount;
    }
    int visibleCount = ( m_nFilteredCount < STORE_MAX_ICONS_IN_RAM ) ? m_nFilteredCount : STORE_MAX_ICONS_IN_RAM;
    SetVisibleRange( 0, visibleCount );
    PreloadNextChunk();
    return true;
}

bool Store::GoToPrevChunk( const char* categoryFilter )
{
    if( m_nChunkApiPage <= 1 ) return false;
    const char* filter = categoryFilter ? categoryFilter : m_currentCategoryFilter.c_str();
    if( !LoadAppsPage( m_nChunkApiPage - 1, filter, -1 ) ) return false;
    m_nDisplayPage = 2;
    return true;
}

//-----------------------------------------------------------------------------
// EnsureVersionsForItem - Fetch versions from API when opening app details
//-----------------------------------------------------------------------------
void Store::EnsureVersionsForItem( StoreItem* pItem )
{
    if( !pItem || !pItem->versions.empty() ) return;
    if( pItem->app.id.empty() ) return;
    VersionsResponse resp;
    if( !WebManager::TryGetVersions( pItem->app.id, resp ) )
    {
        OutputDebugString( "Failed to load versions for app\n" );
        return;
    }
    pItem->versions = resp;
    pItem->nSelectedVersion = 0;
    m_userState.Load( STORE_USER_STATE_PATH );
    m_userState.ApplyToStore( m_pItems, m_nItemCount );
}

//-----------------------------------------------------------------------------
// EnsureScreenshotForItem - Release existing screenshot and try to download for detail view
//-----------------------------------------------------------------------------
void Store::EnsureScreenshotForItem( StoreItem* pItem )
{
    if( !pItem ) return;
    if( pItem->pScreenshot ) {
        pItem->pScreenshot->Release();
        pItem->pScreenshot = NULL;
    }
    pItem->pScreenshot = TextureHelper::GetScreenshot();
    if( !pItem->app.id.empty() )
        m_imageDownloader.Queue( &pItem->pScreenshot, pItem->app.id, IMAGE_SCREENSHOT );
}

//-----------------------------------------------------------------------------
// App download thread and progress
//-----------------------------------------------------------------------------
static void DownloadProgressCallback( uint32_t dlNow, uint32_t dlTotal, void* userData )
{
    Store* s = (Store*)userData;
    if( s ) s->SetDownloadProgress( dlNow, dlTotal );
}

void Store::SetDownloadProgress( uint32_t dlNow, uint32_t dlTotal )
{
    m_downloadNow = dlNow;
    m_downloadTotal = dlTotal;
}

DWORD WINAPI Store::DownloadThreadProc( LPVOID param )
{
    Store* s = (Store*)param;
    if( !s || s->m_downloadVersionId.empty() || s->m_downloadPath.empty() ) return 0;
    CreateDirectory( "T:\\Apps", NULL );
    s->m_downloadSuccess = WebManager::TryDownloadApp( s->m_downloadVersionId, s->m_downloadPath, DownloadProgressCallback, s, &s->m_downloadCancelRequested );
    s->m_downloadDone = true;
    return 0;
}

void Store::StartDownloadThread()
{
    if( m_downloadThread != NULL ) return;
    m_downloadThread = CreateThread( NULL, 0, DownloadThreadProc, this, 0, NULL );
}

void Store::ProcessImageDownloader()
{
    m_imageDownloader.ProcessCompleted( m_pd3dDevice );
}

void Store::BeginDownload( const std::string& versionId, const std::string& appName, const std::string& path )
{
    m_downloadVersionId = versionId;
    m_downloadAppName = appName;
    m_downloadPath = path;
    m_downloadNow = 0;
    m_downloadTotal = 0;
    m_downloadDone = false;
    m_downloadSuccess = false;
    m_downloadCancelRequested = false;
    m_downloadThread = NULL;
}

void Store::CloseDownloadThread()
{
    if( m_downloadThread != NULL )
    {
        WaitForSingleObject( m_downloadThread, INFINITE );
        CloseHandle( m_downloadThread );
        m_downloadThread = NULL;
    }
}

//-----------------------------------------------------------------------------
// LoadCatalogFromWeb - Load categories then first page of apps
//-----------------------------------------------------------------------------
BOOL Store::LoadCatalogFromWeb()
{
    if( !LoadCategoriesFromWeb() )
        return FALSE;
    if( !LoadAppsPage( 1, "", 0 ) )
        return FALSE;
    m_aCategories[0].count = (uint32_t)m_nTotalCount;
    return TRUE;
}

//-----------------------------------------------------------------------------
// MarkAppAsViewed - Clear NEW flag and save state
//-----------------------------------------------------------------------------
void Store::MarkAppAsViewed( const char* appId )
{
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( m_pItems[i].app.id == appId )
        {
            m_pItems[i].app.state = STATE_NONE;
            m_userState.UpdateFromStore( m_pItems, m_nItemCount );
            m_userState.Save( STORE_USER_STATE_PATH );
            OutputDebugString( String::Format( "Marked %s as viewed\n", m_pItems[i].app.name.c_str() ).c_str() );
            return;
        }
    }
}

//-----------------------------------------------------------------------------
// SetVersionState - Change version state and save
//-----------------------------------------------------------------------------
void Store::SetVersionState( const char* appId, const char* version, int state )
{
    /*for( int i = 0; i < m_nItemCount; i++ )
    {
        if( m_pItems[i].app.id != appId ) continue;
        for( size_t v = 0; v < m_pItems[i].versions.size(); v++ )
        {
            if( m_pItems[i].versions[v].version == version )
            {
                m_pItems[i].versions[v].state = state;
                m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                m_userState.Save( STORE_USER_STATE_PATH );
                OutputDebugString( String::Format( "Set %s v%s state to %d\n", m_pItems[i].app.name.c_str(), version, state ).c_str() );
                return;
            }
        }
    }*/
}

BOOL Store::HasUpdateAvailable( StoreItem* pItem ) const
{
    if( !pItem || pItem->versions.size() < 2 )
        return FALSE;
    //int installedIndex = -1;
    //for( size_t v = 0; v < pItem->versions.size(); v++ )
    //{
    //    if( pItem->versions[v].state == STATE_INSTALLED )
    //    {
    //        installedIndex = (int)v;
    //        break;
    //    }
    //}
    //
    //if( installedIndex == -1 ) {
    //    return FALSE;  // Nothing installed
    //}
    //
    //// If installed version is not the first (latest), update available
    //return (installedIndex > 0);
    return FALSE;
}

//-----------------------------------------------------------------------------
// GetDisplayState - Get state to display (with update detection)
// RULE: Only show UPDATE if user actually has the app installed
//-----------------------------------------------------------------------------
uint32_t Store::GetDisplayState( StoreItem* pItem, int versionIndex ) const
{
    return 0;
    //if( !pItem || versionIndex < 0 || versionIndex >= (int)pItem->versions.size() ) {
    //    return 0;
    //}
    //
    //uint32_t actualState = pItem->versions[versionIndex].state;
    //
    //// Card badge (versionIndex == 0): Show best state across ALL versions
    //if( versionIndex == 0 )
    //{
    //    // Priority order:
    //    // 1. Latest version installed -> GREEN
    //    // 2. Any old version installed -> ORANGE (update available)
    //    // 3. Any version downloaded -> BLUE
    //    // 4. Nothing -> GRAY
    //    
    //    BOOL hasDownloaded = FALSE;
    //    int installedIndex = -1;
    //    
    //    for( size_t v = 0; v < pItem->versions.size(); v++ )
    //    {
    //        if( pItem->versions[v].state == STATE_INSTALLED )
    //        {
    //            installedIndex = (int)v;
    //            break;
    //        }
    //        else if( pItem->versions[v].state == STATE_DOWNLOADED ) {
    //            hasDownloaded = TRUE;
    //        }
    //    }
    //    
    //    // If something is installed
    //    if( installedIndex >= 0 )
    //    {
    //        // If latest version (index 0) is installed -> GREEN
    //        if( installedIndex == 0 ) {
    //            return STATE_INSTALLED;  // INSTALLED
    //        }
    //        
    //        // If older version is installed -> ORANGE (update available)
    //        return STATE_NOT_DOWNLOADED;  // UPDATE_AVAILABLE
    //    }
    //    
    //    // Nothing installed, but something downloaded -> BLUE
    //    if( hasDownloaded ) {
    //        return STATE_DOWNLOADED;  // DOWNLOADED
    //    }
    //    
    //    // Nothing at all -> GRAY
    //    return STATE_NONE;  // NOT_DOWNLOADED
    //}
    //
    //// Version list: If this is an old version and it's installed, show UPDATE
    //if( actualState == STATE_INSTALLED && versionIndex > 0 )
    //{
    //    return STATE_NOT_DOWNLOADED;  // UPDATE_AVAILABLE
    //}
    //
    //return actualState;
}

//-----------------------------------------------------------------------------
// BuildFilteredIndices - Fill filtered list by category (for StoreScene)
//-----------------------------------------------------------------------------
void Store::BuildFilteredIndices( int selectedCategory )
{
    m_nFilteredCount = 0;
    if( !m_pFilteredIndices || !m_pItems )
        return;
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( selectedCategory == 0 || m_pItems[i].nCategoryIndex == selectedCategory )
        {
            m_pFilteredIndices[m_nFilteredCount++] = i;
        }
    }
}

//-----------------------------------------------------------------------------
// Visible range: limit icons in RAM; only assign textures for items in range
//-----------------------------------------------------------------------------
void Store::SetVisibleRange( int start, int count )
{
    if( start == m_visibleStart && count == m_visibleCount )
        return;
    m_visibleStart = start;
    m_visibleCount = count;
    ReleaseAllIconsExceptVisible();
    EnsureIconsForVisibleRange();
}

void Store::ReleaseAllIconsExceptVisible()
{
    if( !m_pItems || !m_pFilteredIndices ) return;
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( !m_pItems[i].pIcon ) continue;
        bool inVisible = false;
        for( int s = m_visibleStart; s < m_visibleStart + m_visibleCount && s < m_nFilteredCount; s++ )
        {
            if( m_pFilteredIndices[s] == i ) { inVisible = true; break; }
        }
        if( !inVisible )
        {
            m_pItems[i].pIcon->Release();
            m_pItems[i].pIcon = NULL;
        }
    }
}

void Store::EnsureIconsForVisibleRange()
{
    if( !m_pItems || !m_pFilteredIndices ) return;
    for( int s = m_visibleStart; s < m_visibleStart + m_visibleCount && s < m_nFilteredCount; s++ )
    {
        int itemIndex = m_pFilteredIndices[s];
        StoreItem* pItem = &m_pItems[itemIndex];
        if( pItem->pIcon != NULL )
            continue;
        const std::string& appId = pItem->app.id;
        if( ImageDownloader::IsCoverCached( appId ) )
        {
            std::string path = ImageDownloader::GetCoverCachePath( appId );
            LPDIRECT3DTEXTURE8 pTex = (LPDIRECT3DTEXTURE8)TextureHelper::LoadFromFile( path );
            if( pTex )
                pItem->pIcon = pTex;
            else
            {
                pItem->pIcon = (LPDIRECT3DTEXTURE8)TextureHelper::GetCover();
                m_imageDownloader.Queue( &pItem->pIcon, appId, IMAGE_COVER );
            }
        }
        else
        {
            pItem->pIcon = (LPDIRECT3DTEXTURE8)TextureHelper::GetCover();
            m_imageDownloader.Queue( &pItem->pIcon, appId, IMAGE_COVER );
        }
    }
}

bool Store::IsAppIdInVisibleRange( const std::string& appId ) const
{
    if( !m_pItems || !m_pFilteredIndices ) return false;
    for( int s = m_visibleStart; s < m_visibleStart + m_visibleCount && s < m_nFilteredCount; s++ )
    {
        int itemIndex = m_pFilteredIndices[s];
        if( m_pItems[itemIndex].app.id == appId )
            return true;
    }
    return false;
}

bool Store::OnIconLoadCompletion( void* ctx, LPDIRECT3DTEXTURE8* pOut, const std::string& filePath, const std::string& appId, LPDIRECT3DTEXTURE8 loadedTex )
{
    Store* pStore = (Store*)ctx;
    (void)pOut;
    (void)filePath;
    if( !pStore->IsAppIdInVisibleRange( appId ) )
        return false;
    return true;
}

//-----------------------------------------------------------------------------
// Initialize
//-----------------------------------------------------------------------------
HRESULT Store::Initialize( LPDIRECT3DDEVICE8 pd3dDevice )
{
    m_pd3dDevice = pd3dDevice;
    if( FAILED( TextureHelper::Init( pd3dDevice ) ) )
    {
        OutputDebugString( "Warning: TextureHelper Init failed (Cover/Screenshot).\n" );
    }
    m_imageDownloader.SetCompletionCallback( &Store::OnIconLoadCompletion, this );

    if( !LoadCatalogFromWeb() )
    {
        OutputDebugString( "Failed to load from web\n" );
        m_nItemCount = 0;
        m_nFilteredCount = 0;
        m_pItems = NULL;
        m_pFilteredIndices = NULL;
    }

    m_userState.Load( STORE_USER_STATE_PATH );
    m_userState.ApplyToStore( m_pItems, m_nItemCount );

    return S_OK;
}

