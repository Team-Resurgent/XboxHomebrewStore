//=============================================================================
// Store.cpp - Xbox Homebrew Store Implementation
//=============================================================================

#include "Main.h"
#include "Store.h"
#include "WebManager.h"
#include "String.h"
#include <stdlib.h>
#include <string.h>

// Colors - Modern clean theme inspired by Switch store
#define COLOR_PRIMARY       0xFFE91E63  // Pink/Red accent
#define COLOR_SECONDARY     0xFF424242  // Dark gray
#define COLOR_BG            0xFF212121  // Very dark gray
#define COLOR_CARD_BG       0xFF303030  // Card background
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_TEXT_GRAY     0xFFB0B0B0
#define COLOR_SUCCESS       0xFF4CAF50  // Green for installed
#define COLOR_DOWNLOAD      0xFF2196F3  // Blue for download
#define COLOR_NEW           0xFFFF1744  // Bright red for NEW items
#define COLOR_SIDEBAR       0xFFD81B60  // Sidebar pink
#define COLOR_SIDEBAR_HOVER 0xFFC2185B  // Darker pink for selected

// File paths
#define USER_STATE_PATH     "T:\\user_state.json"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
Store::Store()
{
    m_pItems = NULL;
    m_nItemCount = 0;
    m_nSelectedItem = 0;
    m_nFilteredCount = 0;
    m_CurrentState = UI_MAIN_GRID;
    m_nSelectedCategory = 0;
    m_pVB = NULL;
    m_pVBTex = NULL;
    m_pGamepads = NULL;
    m_dwLastButtons = 0;
    m_pd3dDevice = NULL;
	m_pFilteredIndices = NULL;
    m_downloadNow = 0;
    m_downloadTotal = 0;
    m_downloadDone = false;
    m_downloadSuccess = false;
    m_downloadCancelRequested = false;
    m_downloadThread = NULL;

    // Will be set in Initialize
    m_fScreenWidth = 640.0f;
    m_fScreenHeight = 480.0f;
    m_fSidebarWidth = 200.0f;
    m_nGridCols = 3;
    m_nGridRows = 2;
    
    // Initialize first category as "All Apps"
    CategoryItem allApps;
    allApps.category = "All Apps";
    allApps.count = 0;
    m_aCategories.push_back( allApps );

    m_nCurrentPage = 1;
    m_nTotalPages = 1;
    m_nTotalCount = 0;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
Store::~Store()
{
    if( m_pVB ) {
        m_pVB->Release();
        m_pVB = NULL;
    }
    if( m_pVBTex ) {
        m_pVBTex->Release();
        m_pVBTex = NULL;
    }
    m_Font.Destroy();

	if (m_pFilteredIndices) {
		delete[] m_pFilteredIndices;
	}

    if( m_pItems )
    {
        // Release item textures
        for( int i = 0; i < m_nItemCount; i++ )
        {
            if( m_pItems[i].pIcon ) {
                m_pItems[i].pIcon->Release();
            }
            if( m_pItems[i].pScreenshot ) {
                m_pItems[i].pScreenshot->Release();
            }
        }
        delete[] m_pItems;
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

#define APPS_PAGE_SIZE 8

//-----------------------------------------------------------------------------
// LoadAppsPage - Fetch one page of apps (and optionally filter by category)
//-----------------------------------------------------------------------------
BOOL Store::LoadAppsPage( int page, const char* categoryFilter )
{
    AppsResponse resp;
    std::string catStr = categoryFilter ? categoryFilter : "";
    if( !WebManager::TryGetApps( resp, (uint32_t)page, (uint32_t)APPS_PAGE_SIZE, catStr, "" ) )
    {
        OutputDebugString( "Failed to load apps page from web\n" );
        return FALSE;
    }
    if( resp.items.empty() && page == 1 )
    {
        m_nItemCount = 0;
        m_nFilteredCount = 0;
        m_nCurrentPage = 1;
        m_nTotalPages = 1;
        m_nTotalCount = 0;
        if( m_nSelectedCategory >= 0 && m_nSelectedCategory < (int)m_aCategories.size() ) {
            m_aCategories[m_nSelectedCategory].count = 0;
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
    m_nCurrentPage = (int)resp.page;
    m_nTotalPages = (int)resp.totalPages;
    m_nTotalCount = (int)resp.totalCount;
    if( m_nSelectedCategory >= 0 && m_nSelectedCategory < (int)m_aCategories.size() ) {
        m_aCategories[m_nSelectedCategory].count = (uint32_t)m_nTotalCount;
    }
    m_nSelectedItem = 0;
    m_userState.Load( USER_STATE_PATH );
    m_userState.ApplyToStore( m_pItems, m_nItemCount );
    for( int i = 0; i < m_nItemCount; i++ ) {
        m_pItems[i].pIcon = TextureHelper::GetCover( m_pd3dDevice );
        m_imageDownloader.Queue( &m_pItems[i].pIcon, m_pItems[i].app.id, IMAGE_COVER );
    }
    OutputDebugString( String::Format( "Loaded page %d: %d apps (total %d)\n", m_nCurrentPage, m_nItemCount, m_nTotalCount ).c_str() );
    return TRUE;
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
    m_userState.Load( USER_STATE_PATH );
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
    pItem->pScreenshot = TextureHelper::GetScreenshot( m_pd3dDevice );
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

//-----------------------------------------------------------------------------
// LoadCatalogFromWeb - Load categories then first page of apps
//-----------------------------------------------------------------------------
BOOL Store::LoadCatalogFromWeb()
{
    if( !LoadCategoriesFromWeb() )
        return FALSE;
    m_nSelectedCategory = 0;
    if( !LoadAppsPage( 1, "" ) )
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
            m_pItems[i].app.isNew = false;
            m_userState.UpdateFromStore( m_pItems, m_nItemCount );
            m_userState.Save( USER_STATE_PATH );
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
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( m_pItems[i].app.id != appId ) continue;
        for( size_t v = 0; v < m_pItems[i].versions.size(); v++ )
        {
            if( m_pItems[i].versions[v].version == version )
            {
                m_pItems[i].versions[v].state = state;
                m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                m_userState.Save( USER_STATE_PATH );
                OutputDebugString( String::Format( "Set %s v%s state to %d\n", m_pItems[i].app.name.c_str(), version, state ).c_str() );
                return;
            }
        }
    }
}

BOOL Store::HasUpdateAvailable( StoreItem* pItem )
{
    if( !pItem || pItem->versions.size() < 2 )
        return FALSE;
    int installedIndex = -1;
    for( size_t v = 0; v < pItem->versions.size(); v++ )
    {
        if( pItem->versions[v].state == 2 )
        {
            installedIndex = (int)v;
            break;
        }
    }
    
    if( installedIndex == -1 ) {
        return FALSE;  // Nothing installed
    }
    
    // If installed version is not the first (latest), update available
    return (installedIndex > 0);
}

//-----------------------------------------------------------------------------
// GetDisplayState - Get state to display (with update detection)
// RULE: Only show UPDATE if user actually has the app installed
//-----------------------------------------------------------------------------
int Store::GetDisplayState( StoreItem* pItem, int versionIndex )
{
    if( !pItem || versionIndex < 0 || versionIndex >= (int)pItem->versions.size() ) {
        return 0;
    }
    
    int actualState = pItem->versions[versionIndex].state;
    
    // Card badge (versionIndex == 0): Show best state across ALL versions
    if( versionIndex == 0 )
    {
        // Priority order:
        // 1. Latest version installed -> GREEN
        // 2. Any old version installed -> ORANGE (update available)
        // 3. Any version downloaded -> BLUE
        // 4. Nothing -> GRAY
        
        BOOL hasDownloaded = FALSE;
        int installedIndex = -1;
        
        for( size_t v = 0; v < pItem->versions.size(); v++ )
        {
            if( pItem->versions[v].state == 2 )
            {
                installedIndex = (int)v;
                break;
            }
            else if( pItem->versions[v].state == 1 ) {
                hasDownloaded = TRUE;
            }
        }
        
        // If something is installed
        if( installedIndex >= 0 )
        {
            // If latest version (index 0) is installed -> GREEN
            if( installedIndex == 0 ) {
                return 2;  // INSTALLED
            }
            
            // If older version is installed -> ORANGE (update available)
            return 3;  // UPDATE_AVAILABLE
        }
        
        // Nothing installed, but something downloaded -> BLUE
        if( hasDownloaded ) {
            return 1;  // DOWNLOADED
        }
        
        // Nothing at all -> GRAY
        return 0;  // NOT_DOWNLOADED
    }
    
    // Version list: If this is an old version and it's installed, show UPDATE
    if( actualState == 2 && versionIndex > 0 )
    {
        return 3;  // UPDATE_AVAILABLE
    }
    
    return actualState;
}

//-----------------------------------------------------------------------------
// Initialize
//-----------------------------------------------------------------------------
HRESULT Store::Initialize( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Detect screen resolution
    DetectResolution( pd3dDevice );
    CalculateLayout();
    
    // Create vertex buffer for rendering quads
    if( FAILED( pd3dDevice->CreateVertexBuffer( 4*sizeof(CUSTOMVERTEX),
                                                 D3DUSAGE_WRITEONLY, 
                                                 D3DFVF_CUSTOMVERTEX,
                                                 D3DPOOL_DEFAULT, 
                                                 &m_pVB ) ) )
    {
        return E_FAIL;
    }
    if( FAILED( pd3dDevice->CreateVertexBuffer( 4*sizeof(TEXVERTEX),
                                                 D3DUSAGE_WRITEONLY,
                                                 D3DFVF_TEXVERTEX,
                                                 D3DPOOL_DEFAULT,
                                                 &m_pVBTex ) ) )
    {
        m_pVB->Release();
        m_pVB = NULL;
        return E_FAIL;
    }

    // Initialize font using bundled font files
    if( FAILED( m_Font.Create( "D:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        OutputDebugString( "Warning: Could not load Arial_16.xpr font file.\n" );
    }

    // Initialize gamepads
    XBInput_CreateGamepads( &m_pGamepads );

    m_pd3dDevice = pd3dDevice;
    if( FAILED( TextureHelper::Init( pd3dDevice ) ) ) {
        OutputDebugString( "Warning: TextureHelper Init failed (Cover/Screenshot).\n" );
    }

    // Load app catalog from web (categories + first page of apps)
    if( !LoadCatalogFromWeb() )
    {
        OutputDebugString( "Failed to load from web\n" );
        m_nItemCount = 0;
        m_nFilteredCount = 0;
        m_pItems = NULL;
        m_pFilteredIndices = NULL;
    }
    
    // Load user state
    m_userState.Load( USER_STATE_PATH );
    m_userState.ApplyToStore( m_pItems, m_nItemCount );

    return S_OK;
}

//-----------------------------------------------------------------------------
// Update - Handle input and logic
//-----------------------------------------------------------------------------
void Store::Update()
{
    m_imageDownloader.ProcessCompleted( m_pd3dDevice );
    if( m_CurrentState == UI_DOWNLOADING && m_downloadThread == NULL && !m_downloadDone )
        StartDownloadThread();
    HandleInput();
}

//-----------------------------------------------------------------------------
// Render
//-----------------------------------------------------------------------------
void Store::Render( LPDIRECT3DDEVICE8 pd3dDevice )
{
    switch( m_CurrentState )
    {
        case UI_MAIN_GRID:
            RenderMainGrid( pd3dDevice );
            break;
            
        case UI_ITEM_DETAILS:
            RenderItemDetails( pd3dDevice );
            break;
            
        case UI_DOWNLOADING:
            RenderDownloading( pd3dDevice );
            break;
            
        case UI_SETTINGS:
            RenderSettings( pd3dDevice );
            break;
    }
}

//-----------------------------------------------------------------------------
// RenderCategorySidebar - Left category menu (dynamic)
//-----------------------------------------------------------------------------
void Store::RenderCategorySidebar( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Sidebar background
    DrawRect( pd3dDevice, 0, 0, m_fSidebarWidth, m_fScreenHeight, COLOR_SIDEBAR );
    
    // Header with logo area
    DrawText( pd3dDevice, "Homebrew Store", 20.0f, 20.0f, COLOR_WHITE );
    DrawText( pd3dDevice, "for Xbox", 20.0f, 40.0f, COLOR_WHITE );
    
    // Category menu items (dynamic!)
    float itemY = 100.0f;
    float itemH = 50.0f;
    
    for( size_t i = 0; i < m_aCategories.size(); i++ )
    {
        DWORD bgColor = (i == m_nSelectedCategory) ? COLOR_SIDEBAR_HOVER : COLOR_SIDEBAR;
        
        // Highlight selected category
        if( i == m_nSelectedCategory )
        {
            DrawRect( pd3dDevice, 0, itemY, m_fSidebarWidth, itemH, bgColor );
        }
        
        // Category name with count
        std::string szText = String::Format( "%s (%u)", m_aCategories[i].category.c_str(), (unsigned)m_aCategories[i].count );
        DrawText( pd3dDevice, szText.c_str(), 50.0f, itemY + 15.0f, COLOR_WHITE );
        
        itemY += itemH;
    }
    
    // Hide button at bottom
    DrawText( pd3dDevice, "LB: Hide", 20.0f, m_fScreenHeight - 30.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// RenderMainGrid - Main store browser (Switch-style with sidebar)
//-----------------------------------------------------------------------------
void Store::RenderMainGrid( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Draw category sidebar
    RenderCategorySidebar( pd3dDevice );
    
    // Draw title bar (right of sidebar)
    DrawRect( pd3dDevice, m_fSidebarWidth, 0, m_fScreenWidth - m_fSidebarWidth, 60.0f, COLOR_PRIMARY );
    
    // Get current category name
    const char* categoryName = "All Apps";
    if( m_nSelectedCategory >= 0 && m_nSelectedCategory < (int)m_aCategories.size() ) {
        categoryName = m_aCategories[m_nSelectedCategory].category.c_str();
    }
    
    DrawText( pd3dDevice, categoryName, m_fSidebarWidth + 20.0f, 20.0f, COLOR_WHITE );
    
    // Build filtered indices array for current category
    m_nFilteredCount = 0;
    for( int i = 0; i < m_nItemCount; i++ )
    {
        // "All Apps" (index 0) shows everything
        if( m_nSelectedCategory == 0 || m_pItems[i].nCategoryIndex == m_nSelectedCategory )
        {
            m_pFilteredIndices[m_nFilteredCount++] = i;
        }
    }
    
    // Clamp selection to valid range
    if( m_nSelectedItem >= m_nFilteredCount ) {
        m_nSelectedItem = m_nFilteredCount > 0 ? m_nFilteredCount - 1 : 0;
    }
    
    // Draw grid of app cards
    int visibleItems = m_nGridRows * m_nGridCols;
    int page = m_nSelectedItem / visibleItems;
    int startIndex = page * visibleItems;
    
    for( int row = 0; row < m_nGridRows; row++ )
    {
        for( int col = 0; col < m_nGridCols; col++ )
        {
            int gridIndex = startIndex + (row * m_nGridCols + col);
            if( gridIndex >= m_nFilteredCount ) {
                break;
            }
                
            // Get actual item index from filtered array
            int actualItemIndex = m_pFilteredIndices[gridIndex];
            
            float x = m_fGridStartX + col * (m_fCardWidth + 20.0f);
            float y = m_fGridStartY + row * (m_fCardHeight + 20.0f);
            
            BOOL bSelected = (gridIndex == m_nSelectedItem);
            DrawAppCard( pd3dDevice, &m_pItems[actualItemIndex], x, y, bSelected );
        }
    }

    // Draw bottom instructions bar
    DrawRect( pd3dDevice, m_fSidebarWidth, m_fScreenHeight - 50.0f, 
              m_fScreenWidth - m_fSidebarWidth, 50.0f, COLOR_SECONDARY );
    
    std::string szInstructions;
    if( m_nTotalPages > 1 ) {
        szInstructions = String::Format( "A: Details  B: Exit  LT/RT: Category  D-pad: Move  (%d apps, page %d/%d)", m_nFilteredCount, m_nCurrentPage, m_nTotalPages );
    } else {
        szInstructions = String::Format( "A: Details  B: Exit  LT/RT: Category  (%d apps)", m_nFilteredCount );
    }
    DrawText( pd3dDevice, szInstructions.c_str(), m_fSidebarWidth + 20.0f, m_fScreenHeight - 30.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// RenderItemDetails - Multi-version detail screen with scrollable list
//-----------------------------------------------------------------------------
void Store::RenderItemDetails( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Get actual item from filtered indices
    if( m_nSelectedItem < 0 || m_nSelectedItem >= m_nFilteredCount ) {
        return;
    }
        
    int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
    if( actualItemIndex < 0 || actualItemIndex >= m_nItemCount ) {
        return;
    }

    StoreItem* pItem = &m_pItems[actualItemIndex];
    
    if( pItem->versions.empty() )
    {
        DrawRect( pd3dDevice, 0, 0, m_fScreenWidth, m_fScreenHeight, COLOR_BG );
        DrawText( pd3dDevice, pItem->app.name.c_str(), 20.0f, 20.0f, COLOR_WHITE );
        DrawText( pd3dDevice, "No versions available.", 20.0f, 60.0f, COLOR_TEXT_GRAY );
        DrawText( pd3dDevice, "B: Back", 20.0f, m_fScreenHeight - 40.0f, COLOR_WHITE );
        return;
    }
    
    // Clamp selected version
    if( pItem->nSelectedVersion >= (int)pItem->versions.size() ) {
        pItem->nSelectedVersion = 0;
    }

    VersionItem* pCurrentVersion = &pItem->versions[pItem->nSelectedVersion];

    // Calculate layout
    float sidebarW = m_fScreenWidth * 0.30f;
    if( sidebarW < 200.0f ) sidebarW = 200.0f;
    if( sidebarW > 280.0f ) sidebarW = 280.0f;
    
    float contentW = m_fScreenWidth - sidebarW;
    float actionBarH = 70.0f;
    
    // MODE 1: Viewing specific version detail (like single app)
    if( pItem->bViewingVersionDetail )
    {
        // Left content area
        DrawRect( pd3dDevice, 0, 0, contentW, m_fScreenHeight, COLOR_BG );
        
        // App title and version
        std::string szTitle = String::Format( "%s v%s", pItem->app.name.c_str(), pCurrentVersion->version.c_str() );
        DrawText( pd3dDevice, szTitle.c_str(), 20.0f, 20.0f, COLOR_WHITE );
        DrawText( pd3dDevice, pItem->app.author.c_str(), 20.0f, 40.0f, COLOR_TEXT_GRAY );
        
        // Screenshot
        float screenshotY = 70.0f;
        float screenshotH = m_fScreenHeight * 0.45f;
        DrawRect( pd3dDevice, 20.0f, screenshotY, contentW - 40.0f, screenshotH, COLOR_CARD_BG );
        if( pItem->pScreenshot ) {
            DrawTexturedRect( pd3dDevice, 20.0f, screenshotY, contentW - 40.0f, screenshotH, pItem->pScreenshot );
        }
        float descY = screenshotY + screenshotH + 20.0f;
        
        // Description
        DrawText( pd3dDevice, "Description:", 20.0f, descY, COLOR_TEXT_GRAY );
        descY += 25.0f;
        DrawText( pd3dDevice, pItem->app.description.c_str(), 20.0f, descY, COLOR_WHITE );
        descY += 50.0f;
        
        if( !pCurrentVersion->changeLog.empty() && pCurrentVersion->changeLog != "No changelog available" )
        {
            DrawText( pd3dDevice, "What's New:", 20.0f, descY, COLOR_TEXT_GRAY );
            descY += 25.0f;
            DrawText( pd3dDevice, pCurrentVersion->changeLog.c_str(), 20.0f, descY, COLOR_WHITE );
        }
        
        // Right sidebar - Version metadata
        float sidebarX = contentW;
        DrawRect( pd3dDevice, sidebarX, 0, sidebarW, m_fScreenHeight - actionBarH, COLOR_PRIMARY );
        
        float metaY = 20.0f;
        DrawText( pd3dDevice, "Version:", sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 20.0f;
        DrawText( pd3dDevice, pCurrentVersion->version.c_str(), sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 40.0f;
        
        if( !pCurrentVersion->releaseDate.empty() )
        {
            DrawText( pd3dDevice, "Released:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            DrawText( pd3dDevice, pCurrentVersion->releaseDate.c_str(), sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 40.0f;
        }
        
        DrawText( pd3dDevice, "Size:", sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 20.0f;
        DrawText( pd3dDevice, String::Format( "%.1f MB", pCurrentVersion->size / (1024.0f * 1024.0f) ).c_str(), sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 40.0f;
        
        if( !pCurrentVersion->titleId.empty() )
        {
            DrawText( pd3dDevice, "Title ID:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            DrawText( pd3dDevice, pCurrentVersion->titleId.c_str(), sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 40.0f;
        }
        
        if( !pCurrentVersion->region.empty() )
        {
            DrawText( pd3dDevice, "Region:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            DrawText( pd3dDevice, pCurrentVersion->region.c_str(), sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 40.0f;
        }
        
        int displayState = GetDisplayState( pItem, pItem->nSelectedVersion );
        const char* statusText;
        DWORD statusColor;
        switch( displayState )
        {
            case 2: statusText = "INSTALLED"; statusColor = COLOR_SUCCESS; break;
            case 1: statusText = "DOWNLOADED"; statusColor = COLOR_DOWNLOAD; break;
            case 3: statusText = "UPDATE"; statusColor = 0xFFFF9800; break;
            default: statusText = "NOT DOWNLOADED"; statusColor = COLOR_TEXT_GRAY; break;
        }
        DrawText( pd3dDevice, "Status:", sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 20.0f;
        DrawText( pd3dDevice, statusText, sidebarX + 15.0f, metaY, statusColor );
        metaY += 40.0f;
        
        // Show install path if installed
        if( pCurrentVersion->state == 2 && !pCurrentVersion->install_path.empty() )
        {
            DrawText( pd3dDevice, "Installed:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            
            // Wrap path if too long - show last part
            const char* pathToShow = pCurrentVersion->install_path.c_str();
            if( strlen( pathToShow ) > 20 )
            {
                // Show "E:\Apps\..." format
                std::string shortPath = String::Format( "...%s", pathToShow + strlen(pathToShow) - 15 );
                DrawText( pd3dDevice, shortPath.c_str(), sidebarX + 15.0f, metaY, COLOR_TEXT_GRAY );
            }
            else
            {
                DrawText( pd3dDevice, pathToShow, sidebarX + 15.0f, metaY, COLOR_TEXT_GRAY );
            }
            metaY += 40.0f;
        }
        
        // Action buttons - use ACTUAL state for buttons, not display state
        float actionBarY = m_fScreenHeight - actionBarH;
        DrawRect( pd3dDevice, 0, actionBarY, m_fScreenWidth, actionBarH, COLOR_SECONDARY );
        
        float btnY = actionBarY + 15.0f;
        float btnH = 40.0f;
        float btnSpacing = 20.0f;
        float btnStartX = 40.0f;
        
        // Use actual state for buttons (not displayState which shows UPDATE)
        int actualState = pCurrentVersion->state;
        
        switch( actualState )
        {
            case 0: // NOT_DOWNLOADED
            {
                float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing) / 2.0f;
                DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_DOWNLOAD );
                DrawText( pd3dDevice, "(A) Download", btnStartX + 20.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, COLOR_CARD_BG );
                DrawText( pd3dDevice, "(B) Back", btnStartX + btnW + btnSpacing + 40.0f, btnY + 12.0f, COLOR_WHITE );
                break;
            }
            case 1: // DOWNLOADED
            {
                float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing * 2) / 3.0f;
                DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_SUCCESS );
                DrawText( pd3dDevice, "(A) Install", btnStartX + 15.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, 0xFF9E9E9E );
                DrawText( pd3dDevice, "(X) Delete", btnStartX + btnW + btnSpacing + 15.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + (btnW + btnSpacing) * 2, btnY, btnW, btnH, COLOR_CARD_BG );
                DrawText( pd3dDevice, "(B) Back", btnStartX + (btnW + btnSpacing) * 2 + 20.0f, btnY + 12.0f, COLOR_WHITE );
                break;
            }
            case 2: // INSTALLED
            {
                // If this is an old version (displayState shows UPDATE), show Launch + Uninstall
                // If this is latest version, show Launch + Uninstall
                float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing * 2) / 3.0f;
                DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_SUCCESS );
                DrawText( pd3dDevice, "(A) Launch", btnStartX + 15.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, 0xFFD32F2F );
                DrawText( pd3dDevice, "(X) Uninstall", btnStartX + btnW + btnSpacing + 8.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + (btnW + btnSpacing) * 2, btnY, btnW, btnH, COLOR_CARD_BG );
                DrawText( pd3dDevice, "(B) Back", btnStartX + (btnW + btnSpacing) * 2 + 20.0f, btnY + 12.0f, COLOR_WHITE );
                break;
            }
        }
        
        return; // Done with single version detail view
    }
    
    // MODE 2: Version list view (for multiple versions)
    DrawRect( pd3dDevice, 0, 0, contentW, m_fScreenHeight, COLOR_BG );
    
    DrawText( pd3dDevice, pItem->app.name.c_str(), 20.0f, 20.0f, COLOR_WHITE );
    DrawText( pd3dDevice, pItem->app.author.c_str(), 20.0f, 40.0f, COLOR_TEXT_GRAY );
    
    // Screenshot
    float screenshotY = 70.0f;
    float screenshotH = m_fScreenHeight * 0.30f;
    DrawRect( pd3dDevice, 20.0f, screenshotY, contentW - 40.0f, screenshotH, COLOR_CARD_BG );
    if( pItem->pScreenshot ) {
        DrawTexturedRect( pd3dDevice, 20.0f, screenshotY, contentW - 40.0f, screenshotH, pItem->pScreenshot );
    }
    float contentY = screenshotY + screenshotH + 20.0f;

    DrawText( pd3dDevice, "Description:", 20.0f, contentY, COLOR_TEXT_GRAY );
    contentY += 25.0f;
    DrawText( pd3dDevice, pItem->app.description.c_str(), 20.0f, contentY, COLOR_WHITE );
    contentY += 50.0f;
    
    if( pItem->versions.size() > 1 )
    {
        DrawText( pd3dDevice, "Available Versions (UP/DOWN to browse, A to view):", 20.0f, contentY, COLOR_TEXT_GRAY );
        contentY += 30.0f;
        
        // Calculate visible items (max 3-4 depending on space)
        int maxVisible = 3;
        
        // Adjust scroll offset
        if( pItem->nSelectedVersion < pItem->nVersionScrollOffset ) {
            pItem->nVersionScrollOffset = pItem->nSelectedVersion;
        }
        if( pItem->nSelectedVersion >= pItem->nVersionScrollOffset + maxVisible ) {
            pItem->nVersionScrollOffset = pItem->nSelectedVersion - maxVisible + 1;
        }
        
        // Show "More above" indicator BEFORE the list
        if( pItem->nVersionScrollOffset > 0 )
        {
            DrawText( pd3dDevice, "^ More above", 30.0f, contentY, COLOR_TEXT_GRAY );
            contentY += 20.0f;
        }
        
        float listStartY = contentY;
        
        // Draw visible versions
        for( int i = 0; i < maxVisible && (i + pItem->nVersionScrollOffset) < (int)pItem->versions.size(); i++ )
        {
            int vIdx = i + pItem->nVersionScrollOffset;
            VersionItem* pVer = &pItem->versions[vIdx];
            BOOL bSelected = (vIdx == pItem->nSelectedVersion);
            
            float itemH = 55.0f;
            DWORD bgColor = bSelected ? COLOR_PRIMARY : COLOR_CARD_BG;
            
            DrawRect( pd3dDevice, 20.0f, contentY, contentW - 40.0f, itemH, bgColor );
            
            std::string szVer = String::Format( "v%s", pVer->version.c_str() );
            DrawText( pd3dDevice, szVer.c_str(), 30.0f, contentY + 8.0f, COLOR_WHITE );
            
            if( !pVer->releaseDate.empty() )
                DrawText( pd3dDevice, pVer->releaseDate.c_str(), 30.0f, contentY + 28.0f, COLOR_TEXT_GRAY );
            
            std::string szSize = String::Format( "%.1f MB", pVer->size / (1024.0f * 1024.0f) );
            DrawText( pd3dDevice, szSize.c_str(), contentW - 120.0f, contentY + 18.0f, COLOR_WHITE );
            
            if( pVer->state == 2 )
            {
                DrawText( pd3dDevice, "INSTALLED", contentW - 220.0f, contentY + 18.0f, COLOR_SUCCESS );
            }
            else if( pVer->state == 1 )
            {
                DrawText( pd3dDevice, "DOWNLOADED", contentW - 240.0f, contentY + 18.0f, COLOR_DOWNLOAD );
            }
            
            contentY += itemH + 5.0f;
        }
        
        // Show "More below" indicator AFTER the list
        if( pItem->nVersionScrollOffset + maxVisible < (int)pItem->versions.size() )
        {
            DrawText( pd3dDevice, "v More below", 30.0f, contentY + 5.0f, COLOR_TEXT_GRAY );
        }
    }
    
    // Right sidebar
    float sidebarX = contentW;
    DrawRect( pd3dDevice, sidebarX, 0, sidebarW, m_fScreenHeight - actionBarH, COLOR_PRIMARY );
    
    float metaY = 20.0f;
    DrawText( pd3dDevice, "Title:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, pItem->app.name.c_str(), sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    DrawText( pd3dDevice, "Author:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, pItem->app.author.c_str(), sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    DrawText( pd3dDevice, String::Format( "%u version(s)", (unsigned)pItem->versions.size() ).c_str(), sidebarX + 15.0f, metaY, COLOR_TEXT_GRAY );
    
    // Bottom bar
    float actionBarY = m_fScreenHeight - actionBarH;
    DrawRect( pd3dDevice, 0, actionBarY, m_fScreenWidth, actionBarH, COLOR_SECONDARY );
    DrawText( pd3dDevice, "(A) View Version  (B) Back to Grid", 40.0f, actionBarY + 25.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// RenderDownloading - Download progress screen (Switch-style)
//-----------------------------------------------------------------------------
void Store::RenderDownloading( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Title bar
    DrawRect( pd3dDevice, 0, 0, m_fScreenWidth, 60.0f, COLOR_PRIMARY );
    DrawText( pd3dDevice, "DOWNLOADING", 20.0f, 20.0f, COLOR_WHITE );

    // Center panel
    float panelW = m_fScreenWidth * 0.7f;
    if( panelW > 500.0f ) panelW = 500.0f;
    float panelH = 200.0f;
    float panelX = (m_fScreenWidth - panelW) / 2.0f;
    float panelY = (m_fScreenHeight - panelH) / 2.0f;
    
    DrawRect( pd3dDevice, panelX, panelY, panelW, panelH, COLOR_CARD_BG );
    
    DrawText( pd3dDevice, m_downloadAppName.c_str(), panelX + 20.0f, panelY + 20.0f, COLOR_WHITE );
    
    float barY = panelY + 80.0f;
    float barW = panelW - 40.0f;
    DrawRect( pd3dDevice, panelX + 20.0f, barY, barW, 30.0f, COLOR_SECONDARY );
    float pct = (m_downloadTotal > 0) ? (float)m_downloadNow / (float)m_downloadTotal : 0.0f;
    if( pct > 1.0f ) pct = 1.0f;
    DrawRect( pd3dDevice, panelX + 22.0f, barY + 2.0f, (barW - 4.0f) * pct, 26.0f, COLOR_DOWNLOAD );
    
    if( m_downloadDone )
    {
        DrawText( pd3dDevice, m_downloadSuccess ? "Complete" : "Failed", panelX + 20.0f, barY + 50.0f, m_downloadSuccess ? COLOR_SUCCESS : COLOR_NEW );
        DrawText( pd3dDevice, "B: Back", panelX + 20.0f, barY + 70.0f, COLOR_TEXT_GRAY );
    }
    else
    {
        DrawText( pd3dDevice, String::Format( "%u%%", m_downloadTotal > 0 ? (unsigned)( pct * 100.0f ) : 0u ).c_str(), panelX + 20.0f, barY + 50.0f, COLOR_WHITE );
        DrawText( pd3dDevice, String::Format( "%u KB / %u KB", (unsigned)( m_downloadNow / 1024 ), (unsigned)( m_downloadTotal / 1024 ) ).c_str(), panelX + 20.0f, barY + 70.0f, COLOR_TEXT_GRAY );
        DrawText( pd3dDevice, "B: Cancel", panelX + 20.0f, barY + 95.0f, COLOR_TEXT_GRAY );
    }
}

//-----------------------------------------------------------------------------
// RenderSettings - Settings screen (Switch-style)
//-----------------------------------------------------------------------------
void Store::RenderSettings( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Title bar
    DrawRect( pd3dDevice, 0, 0, m_fScreenWidth, 60.0f, COLOR_PRIMARY );
    DrawText( pd3dDevice, "SETTINGS", 20.0f, 20.0f, COLOR_WHITE );

    // Menu items
    float itemY = 100.0f;
    float itemH = 50.0f;
    float itemW = m_fScreenWidth - 80.0f;
    
    // Network Settings
    DrawRect( pd3dDevice, 40.0f, itemY, itemW, itemH, COLOR_CARD_BG );
    DrawText( pd3dDevice, "Network Settings", 60.0f, itemY + 15.0f, COLOR_WHITE );
    itemY += itemH + 10.0f;
    
    // Storage Management
    DrawRect( pd3dDevice, 40.0f, itemY, itemW, itemH, COLOR_CARD_BG );
    DrawText( pd3dDevice, "Storage Management", 60.0f, itemY + 15.0f, COLOR_WHITE );
    itemY += itemH + 10.0f;
    
    // About
    DrawRect( pd3dDevice, 40.0f, itemY, itemW, itemH, COLOR_CARD_BG );
    DrawText( pd3dDevice, "About", 60.0f, itemY + 15.0f, COLOR_WHITE );

    // Bottom bar
    DrawRect( pd3dDevice, 0, m_fScreenHeight - 50.0f, m_fScreenWidth, 50.0f, COLOR_SECONDARY );
    DrawText( pd3dDevice, "B: Back", 20.0f, m_fScreenHeight - 30.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// DetectResolution - Get the current display mode resolution
//-----------------------------------------------------------------------------
void Store::DetectResolution( LPDIRECT3DDEVICE8 pd3dDevice )
{
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory( &pp, sizeof(pp) );
    
    // Get current backbuffer dimensions
    LPDIRECT3DSURFACE8 pBackBuffer = NULL;
    if( SUCCEEDED( pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer ) ) )
    {
        D3DSURFACE_DESC desc;
        if( SUCCEEDED( pBackBuffer->GetDesc( &desc ) ) )
        {
            m_fScreenWidth = (float)desc.Width;
            m_fScreenHeight = (float)desc.Height;
            
            OutputDebugString( String::Format( "Detected resolution: %.0fx%.0f\n", m_fScreenWidth, m_fScreenHeight ).c_str() );
        }
        pBackBuffer->Release();
    }
    
    // Fallback to common Xbox resolutions if detection failed
    if( m_fScreenWidth < 100.0f )
    {
        m_fScreenWidth = 640.0f;
        m_fScreenHeight = 480.0f;
        OutputDebugString( "Using default 640x480 resolution\n" );
    }
}

//-----------------------------------------------------------------------------
// CalculateLayout - Calculate dynamic layout based on resolution
//-----------------------------------------------------------------------------
void Store::CalculateLayout()
{
    // Sidebar width scales with resolution
    m_fSidebarWidth = m_fScreenWidth * 0.25f;  // 25% of screen width
    if( m_fSidebarWidth < 180.0f ) m_fSidebarWidth = 180.0f;
    if( m_fSidebarWidth > 250.0f ) m_fSidebarWidth = 250.0f;
    
    // Calculate grid dimensions
    float contentWidth = m_fScreenWidth - m_fSidebarWidth - 80.0f; // Minus margins
    float contentHeight = m_fScreenHeight - 170.0f; // Minus header and footer
    
    // Determine grid layout based on resolution
    if( m_fScreenWidth >= 1280.0f )
    {
        // HD resolutions: 4x2 grid
        m_nGridCols = 4;
        m_nGridRows = 2;
    }
    else if( m_fScreenWidth >= 1024.0f )
    {
        // 1024x768: 3x2 grid
        m_nGridCols = 3;
        m_nGridRows = 2;
    }
    else if( m_fScreenWidth >= 720.0f )
    {
        // 720p: 3x2 grid
        m_nGridCols = 3;
        m_nGridRows = 2;
    }
    else
    {
        // 480p/640x480: 2x2 grid
        m_nGridCols = 2;
        m_nGridRows = 2;
    }
    
    // Calculate card size based on available space
    float cardSpacing = 20.0f;
    m_fCardWidth = (contentWidth - (m_nGridCols + 1) * cardSpacing) / m_nGridCols;
    m_fCardHeight = (contentHeight - (m_nGridRows + 1) * cardSpacing) / m_nGridRows;
    
    // Set grid starting position
    m_fGridStartX = m_fSidebarWidth + 40.0f;
    m_fGridStartY = 100.0f;
    
    OutputDebugString( String::Format( "Layout: %dx%d grid, cards %.0fx%.0f, sidebar %.0f\n",
             m_nGridCols, m_nGridRows, m_fCardWidth, m_fCardHeight, m_fSidebarWidth ).c_str() );
}

//-----------------------------------------------------------------------------
// DrawRect - Draw a filled rectangle
//-----------------------------------------------------------------------------
void Store::DrawRect( LPDIRECT3DDEVICE8 pd3dDevice, float x, float y, float w, float h, DWORD color )
{
    CUSTOMVERTEX vertices[] =
    {
        { x,     y,     0.5f, 1.0f, color },
        { x + w, y,     0.5f, 1.0f, color },
        { x,     y + h, 0.5f, 1.0f, color },
        { x + w, y + h, 0.5f, 1.0f, color },
    };

    VOID* pVertices;
    if( FAILED( m_pVB->Lock( 0, sizeof(vertices), (BYTE**)&pVertices, 0 ) ) )
        return;
    memcpy( pVertices, vertices, sizeof(vertices) );
    m_pVB->Unlock();

    pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(CUSTOMVERTEX) );
    pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
    pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
}

//-----------------------------------------------------------------------------
// DrawTexturedRect - Draw a textured quad (for icons)
//-----------------------------------------------------------------------------
void Store::DrawTexturedRect( LPDIRECT3DDEVICE8 pd3dDevice, float x, float y, float w, float h, LPDIRECT3DTEXTURE8 pTexture )
{
    if( !pTexture || !m_pVBTex ) return;
    TEXVERTEX vertices[] =
    {
        { x,     y,     0.5f, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f },
        { x + w, y,     0.5f, 1.0f, 0xFFFFFFFF, 1.0f, 0.0f },
        { x,     y + h, 0.5f, 1.0f, 0xFFFFFFFF, 0.0f, 1.0f },
        { x + w, y + h, 0.5f, 1.0f, 0xFFFFFFFF, 1.0f, 1.0f },
    };
    VOID* pV;
    if( FAILED( m_pVBTex->Lock( 0, sizeof(vertices), (BYTE**)&pV, 0 ) ) ) return;
    memcpy( pV, vertices, sizeof(vertices) );
    m_pVBTex->Unlock();

    pd3dDevice->SetTexture( 0, pTexture );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    pd3dDevice->SetStreamSource( 0, m_pVBTex, sizeof(TEXVERTEX) );
    pd3dDevice->SetVertexShader( D3DFVF_TEXVERTEX );
    pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
    pd3dDevice->SetTexture( 0, NULL );
}

//-----------------------------------------------------------------------------
// DrawText - Draw text using CXBFont
//-----------------------------------------------------------------------------
void Store::DrawText( LPDIRECT3DDEVICE8 pd3dDevice, const char* text, float x, float y, DWORD color )
{
    // Convert to wide string for CXBFont
    WCHAR wstr[256];
    MultiByteToWideChar( CP_ACP, 0, text, -1, wstr, 256 );
    
    m_Font.DrawText( x, y, color, wstr, XBFONT_LEFT );
}

//-----------------------------------------------------------------------------
// DrawAppCard - Draw a single app card (Switch-style, dynamic size)
//-----------------------------------------------------------------------------
void Store::DrawAppCard( LPDIRECT3DDEVICE8 pd3dDevice, StoreItem* pItem, float x, float y, BOOL bSelected )
{
    // Card background - normal color always
    DWORD cardColor = bSelected ? COLOR_SECONDARY : COLOR_CARD_BG;
    DrawRect( pd3dDevice, x, y, m_fCardWidth, m_fCardHeight, cardColor );
    
    // Selection highlight border
    if( bSelected )
    {
        DrawRect( pd3dDevice, x - 3, y - 3, m_fCardWidth + 6, m_fCardHeight + 6, COLOR_PRIMARY );
        DrawRect( pd3dDevice, x, y, m_fCardWidth, m_fCardHeight, cardColor );
    }
    
    // App icon/thumbnail area (square, as large as possible)
    float thumbSize = m_fCardWidth - 20.0f;
    if( thumbSize > m_fCardHeight - 60.0f ) // Leave room for text
        thumbSize = m_fCardHeight - 60.0f;
    
    DrawRect( pd3dDevice, x + 10, y + 10, thumbSize, thumbSize, COLOR_BG );
    if( pItem->pIcon ) {
        DrawTexturedRect( pd3dDevice, x + 10, y + 10, thumbSize, thumbSize, pItem->pIcon );
    }

    // Status badge (top-right corner) - RED if new, otherwise varies by state
    DWORD badgeColor;
    
    if( pItem->app.isNew )
    {
        // NEW items get bright red badge
        badgeColor = COLOR_NEW;
    }
    else
    {
        // Normal state colors - use display state for update detection
        int state = (!pItem->versions.empty()) ? GetDisplayState( pItem, 0 ) : 0;
        
        switch( state )
        {
            case 2: // STATE_INSTALLED
                badgeColor = COLOR_SUCCESS; // Green checkmark
                break;
            case 1: // STATE_DOWNLOADED
                badgeColor = COLOR_DOWNLOAD; // Blue - ready to install
                break;
            case 3: // STATE_UPDATE_AVAILABLE
                badgeColor = 0xFFFF9800; // Orange - update available
                break;
            default: // STATE_NOT_DOWNLOADED
                badgeColor = COLOR_TEXT_GRAY; // Gray - not downloaded
                break;
        }
    }
    DrawRect( pd3dDevice, x + m_fCardWidth - 35, y + 10, 25, 25, badgeColor );

    // App name below thumbnail
    DrawText( pd3dDevice, pItem->app.name.c_str(), x + 10, y + thumbSize + 15, COLOR_WHITE );
    
    if( m_fCardHeight > 180.0f )
    {
        DrawText( pd3dDevice, pItem->app.author.c_str(), x + 10, y + thumbSize + 35, COLOR_TEXT_GRAY );
    }
}

//-----------------------------------------------------------------------------
// HandleInput - Process controller input
//-----------------------------------------------------------------------------
void Store::HandleInput()
{
    // Get input from all gamepads
    XBInput_GetInput( m_pGamepads );
    
    // Use gamepad 0
    XBGAMEPAD* pGamepad = &m_pGamepads[0];
    
    // Get pressed buttons (buttons that just went down this frame)
    WORD wPressed = pGamepad->wPressedButtons;

    if( m_CurrentState == UI_MAIN_GRID )
    {
        // Left trigger - previous category
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] )
        {
            m_nSelectedCategory--;
            if( m_nSelectedCategory < 0 ) { m_nSelectedCategory = (int)m_aCategories.size() - 1; }
            m_nSelectedItem = 0;
            const char* catFilter = ( m_nSelectedCategory == 0 ) ? "" : m_aCategories[m_nSelectedCategory].category.c_str();
            LoadAppsPage( 1, catFilter );
        }
        
        // Right trigger - next category
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
        {
            m_nSelectedCategory++;
            if( m_nSelectedCategory >= (int)m_aCategories.size() ) { m_nSelectedCategory = 0; }
            m_nSelectedItem = 0;
            const char* catFilter = ( m_nSelectedCategory == 0 ) ? "" : m_aCategories[m_nSelectedCategory].category.c_str();
            LoadAppsPage( 1, catFilter );
        }
        
        // Navigate grid with D-pad (auto-load next/prev page when moving past edge)
        const char* catFilter = ( m_nSelectedCategory == 0 ) ? "" : m_aCategories[m_nSelectedCategory].category.c_str();
        if( wPressed & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            if( m_nSelectedItem % m_nGridCols > 0 ) {
                m_nSelectedItem--;
            }
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            if( m_nSelectedItem % m_nGridCols < m_nGridCols - 1 ) {
                m_nSelectedItem++;
            }
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_UP )
        {
            if( m_nSelectedItem >= m_nGridCols )
            {
                m_nSelectedItem -= m_nGridCols;
            }
            else if( m_nCurrentPage > 1 )
            {
                if( LoadAppsPage( m_nCurrentPage - 1, catFilter ) ) {
                    m_nSelectedItem = m_nFilteredCount > 0 ? m_nFilteredCount - 1 : 0;
                }
            }
            else
            {
                m_nSelectedItem = 0;
            }
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            int nextRow = m_nSelectedItem + m_nGridCols;
            if( nextRow < m_nFilteredCount )
            {
                m_nSelectedItem = nextRow;
            }
            else if( m_nCurrentPage < m_nTotalPages )
            {
                if( LoadAppsPage( m_nCurrentPage + 1, catFilter ) )
                    m_nSelectedItem = 0;
            }
            else
            {
                m_nSelectedItem = m_nFilteredCount > 0 ? m_nFilteredCount - 1 : 0;
            }
        }

        // A button - select item
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        {
            if( m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
            {
                int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
                if( actualItemIndex >= 0 && actualItemIndex < m_nItemCount )
                {
                    EnsureVersionsForItem( &m_pItems[actualItemIndex] );
                    m_pItems[actualItemIndex].app.isNew = false;
                    m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                    m_userState.Save( USER_STATE_PATH );
                    StoreItem* pItem = &m_pItems[actualItemIndex];
                    if( pItem->versions.size() == 1 ) {
                        pItem->bViewingVersionDetail = TRUE;
                        EnsureScreenshotForItem( pItem );
                    } else {
                        pItem->bViewingVersionDetail = FALSE;
                        EnsureScreenshotForItem( pItem );
                    }
                }
            }
            m_CurrentState = UI_ITEM_DETAILS;
        }

        // Y button - settings
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        {
            m_CurrentState = UI_SETTINGS;
        }
    }
    else if( m_CurrentState == UI_ITEM_DETAILS )
    {
        // Get actual item index
        if( m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
        {
            int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
            StoreItem* pItem = &m_pItems[actualItemIndex];
            
            // B button - back
            if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
            {
                if( pItem->bViewingVersionDetail )
                {
                    // If single version, go back to grid
                    // If multi version, go back to version list
                    if( pItem->versions.size() == 1 )
                    {
                        m_CurrentState = UI_MAIN_GRID;
                    }
                    else
                    {
                        // Back to version list
                        pItem->bViewingVersionDetail = FALSE;
                    }
                }
                else
                {
                    // Back to grid
                    m_CurrentState = UI_MAIN_GRID;
                }
            }
            
            // If viewing version detail, handle actions for that specific version
            if( pItem->bViewingVersionDetail )
            {
                VersionItem* pVer = &pItem->versions[pItem->nSelectedVersion];
                
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    switch( pVer->state )
                    {
                        case 0:
                            m_downloadVersionId = pVer->id;
                            m_downloadAppName = pItem->app.name;
                            m_downloadPath = "T:\\Apps\\" + pItem->app.name + ".zip";
                            m_downloadNow = 0;
                            m_downloadTotal = 0;
                            m_downloadDone = false;
                            m_downloadCancelRequested = false;
                            m_downloadThread = NULL;
                            m_CurrentState = UI_DOWNLOADING;
                            break; // Download
                        case 1:  // Install (this IS the update if newer version exists!)
                        {
                            // Generate install path: E:\Apps\AppName_Version
                            char appName[64];
                            // Remove spaces and special chars from app name
                            int j = 0;
                            for( const char* s = pItem->app.name.c_str(); *s && j < 63; s++ )
                            {
                                if( (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') )
                                    appName[j++] = *s;
                            }
                            appName[j] = '\0';
                            
                            // Remove dots from version for path
                            char verClean[16];
                            j = 0;
                            for( const char* s = pVer->version.c_str(); *s && j < 15; s++ )
                            {
                                if( *s != '.' && *s != ' ' ) {
                                    verClean[j++] = *s;
                                }
                            }
                            verClean[j] = '\0';
                            
                            // Create install path
                            pVer->install_path = "T:\\Apps\\";
                            pVer->install_path += appName;
                            pVer->install_path += "_";
                            pVer->install_path += verClean;
                            pVer->state = 2;
                            m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                            m_userState.Save( USER_STATE_PATH );
                            OutputDebugString( String::Format( "Installed to: %s\n", pVer->install_path.c_str() ).c_str() );
                            break;
                        }
                        case 2: /* Launch */ break;
                    }
                }
                
                // X button - delete/uninstall
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
                {
                    if( pVer->state == 1 || pVer->state == 2 )
                    {
                        pVer->install_path.clear();
                        pVer->state = 0;
                        m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                        m_userState.Save( USER_STATE_PATH );
                        
                        OutputDebugString( "Uninstalled and deleted folder\n" );
                    }
                }
            }
            // If viewing version list
            else if( pItem->versions.size() > 1 )
            {
                if( wPressed & XINPUT_GAMEPAD_DPAD_UP )
                {
                    pItem->nSelectedVersion--;
                    if( pItem->nSelectedVersion < 0 ) {
                        pItem->nSelectedVersion = (int)pItem->versions.size() - 1;
                    }
                }
                if( wPressed & XINPUT_GAMEPAD_DPAD_DOWN )
                {
                    pItem->nSelectedVersion++;
                    if( pItem->nSelectedVersion >= (int)pItem->versions.size() ) {
                        pItem->nSelectedVersion = 0;
                    }
                }
                
                // A button - view selected version details
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    pItem->bViewingVersionDetail = TRUE;
                    EnsureScreenshotForItem( pItem );
                }
            }
            // Single version - act immediately
            else
            {
                VersionItem* pVer = &pItem->versions[0];
                
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    switch( pVer->state )
                    {
                        case 0:
                            m_downloadVersionId = pVer->id;
                            m_downloadAppName = pItem->app.name;
                            m_downloadPath = "T:\\Apps\\" + pItem->app.name + ".zip";
                            m_downloadNow = 0;
                            m_downloadTotal = 0;
                            m_downloadDone = false;
                            m_downloadCancelRequested = false;
                            m_downloadThread = NULL;
                            m_CurrentState = UI_DOWNLOADING;
                            break;
                        case 1:
                        {
                            char appName[64];
                            int j = 0;
                            for( const char* s = pItem->app.name.c_str(); *s && j < 63; s++ )
                            {
                                if( (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') )
                                    appName[j++] = *s;
                            }
                            appName[j] = '\0';
                            char verClean[16];
                            j = 0;
                            for( const char* s = pVer->version.c_str(); *s && j < 15; s++ )
                            {
                                if( *s != '.' && *s != ' ' ) {
                                    verClean[j++] = *s;
                                }
                            }
                            verClean[j] = '\0';
                            pVer->install_path = "T:\\Apps\\";
                            pVer->install_path += appName;
                            pVer->install_path += "_";
                            pVer->install_path += verClean;
                            pVer->state = 2;
                            m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                            m_userState.Save( USER_STATE_PATH );
                            break;
                        }
                        case 2: break;
                    }
                }
                
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
                {
                    if( pVer->state == 1 || pVer->state == 2 )
                    {
                        pVer->install_path.clear();
                        pVer->state = 0;
                        m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                        m_userState.Save( USER_STATE_PATH );
                    }
                }
            }
        }
    }
    else if( m_CurrentState == UI_DOWNLOADING )
    {
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        {
            if( m_downloadDone )
            {
                if( m_downloadThread != NULL )
                {
                    WaitForSingleObject( m_downloadThread, INFINITE );
                    CloseHandle( m_downloadThread );
                    m_downloadThread = NULL;
                }
                if( m_downloadSuccess && m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
                {
                    int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
                    if( actualItemIndex >= 0 && actualItemIndex < m_nItemCount )
                    {
                        StoreItem* pItem = &m_pItems[actualItemIndex];
                        if( pItem->nSelectedVersion >= 0 && pItem->nSelectedVersion < (int)pItem->versions.size() )
                        {
                            pItem->versions[pItem->nSelectedVersion].state = 1;
                            m_userState.UpdateFromStore( m_pItems, m_nItemCount );
                            m_userState.Save( USER_STATE_PATH );
                        }
                    }
                }
                m_CurrentState = UI_ITEM_DETAILS;
            }
            else
                m_downloadCancelRequested = true;
        }
    }
    else if( m_CurrentState == UI_SETTINGS )
    {
        // B button - back
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        {
            m_CurrentState = UI_MAIN_GRID;
        }
    }
}