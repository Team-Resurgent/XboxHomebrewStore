//=============================================================================
// Store.cpp - Xbox Homebrew Store Implementation
//=============================================================================

#include "Store.h"
#include <stdio.h>

// Colors - Modern clean theme inspired by Switch store
#define COLOR_PRIMARY       0xFFE91E63  // Pink/Red accent
#define COLOR_SECONDARY     0xFF424242  // Dark gray
#define COLOR_BG            0xFF212121  // Very dark gray
#define COLOR_CARD_BG       0xFF303030  // Card background
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_TEXT_GRAY     0xFFB0B0B0
#define COLOR_SUCCESS       0xFF4CAF50  // Green for installed
#define COLOR_DOWNLOAD      0xFF2196F3  // Blue for download
#define COLOR_SIDEBAR       0xFFD81B60  // Sidebar pink
#define COLOR_SIDEBAR_HOVER 0xFFC2185B  // Darker pink for selected

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
    m_nCategoryCount = 0;
    m_nSelectedCategory = 0;
    m_pVB = NULL;
    m_pGamepads = NULL;
    m_dwLastButtons = 0;
    
    // Will be set in Initialize
    m_fScreenWidth = 640.0f;
    m_fScreenHeight = 480.0f;
    m_fSidebarWidth = 200.0f;
    m_nGridCols = 3;
    m_nGridRows = 2;
    
    // Initialize first category as "All Apps"
    strcpy( m_aCategories[0].szName, "All Apps" );
    strcpy( m_aCategories[0].szID, "all" );
    m_aCategories[0].nAppCount = 0;
    m_nCategoryCount = 1;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
Store::~Store()
{
    if( m_pVB )
        m_pVB->Release();
    
    m_Font.Destroy();

    if( m_pItems )
    {
        // Release item textures
        for( int i = 0; i < m_nItemCount; i++ )
        {
            if( m_pItems[i].pIcon )
                m_pItems[i].pIcon->Release();
        }
        delete[] m_pItems;
    }
}

//-----------------------------------------------------------------------------
// Simple JSON helper - find value for a key
//-----------------------------------------------------------------------------
const char* Store::FindJSONValue( const char* json, const char* key )
{
    static char value[512];
    value[0] = '\0';
    
    char searchKey[128];
    sprintf( searchKey, "\"%s\"", key );
    
    const char* keyPos = strstr( json, searchKey );
    if( !keyPos ) return value;
    
    const char* colonPos = strchr( keyPos, ':' );
    if( !colonPos ) return value;
    
    // Skip whitespace after colon
    colonPos++;
    while( *colonPos == ' ' || *colonPos == '\t' ) colonPos++;
    
    // Handle string values (in quotes)
    if( *colonPos == '"' )
    {
        colonPos++;
        int i = 0;
        while( *colonPos && *colonPos != '"' && i < 510 )
        {
            value[i++] = *colonPos++;
        }
        value[i] = '\0';
    }
    // Handle numeric values
    else
    {
        int i = 0;
        while( *colonPos && (*colonPos == '-' || (*colonPos >= '0' && *colonPos <= '9')) && i < 510 )
        {
            value[i++] = *colonPos++;
        }
        value[i] = '\0';
    }
    
    return value;
}

//-----------------------------------------------------------------------------
// GetOrCreateCategory - Find or create a category index
//-----------------------------------------------------------------------------
int Store::GetOrCreateCategory( const char* catID )
{
    // Check if category already exists
    for( int i = 1; i < m_nCategoryCount; i++ ) // Start at 1 to skip "All Apps"
    {
        if( strcmp( m_aCategories[i].szID, catID ) == 0 )
            return i;
    }
    
    // Create new category if we have room
    if( m_nCategoryCount >= MAX_CATEGORIES )
    {
        OutputDebugString( "Warning: Max categories reached\n" );
        return 1; // Return first category after "All"
    }
    
    int newIndex = m_nCategoryCount++;
    strcpy( m_aCategories[newIndex].szID, catID );
    
    // Capitalize first letter for display name
    strcpy( m_aCategories[newIndex].szName, catID );
    if( m_aCategories[newIndex].szName[0] >= 'a' && m_aCategories[newIndex].szName[0] <= 'z' )
        m_aCategories[newIndex].szName[0] -= 32; // Uppercase first letter
    
    m_aCategories[newIndex].nAppCount = 0;
    
    char szDebug[256];
    sprintf( szDebug, "Created new category: %s (index %d)\n", catID, newIndex );
    OutputDebugString( szDebug );
    
    return newIndex;
}

//-----------------------------------------------------------------------------
// BuildCategoryList - Count apps in each category
//-----------------------------------------------------------------------------
void Store::BuildCategoryList()
{
    // Reset counts
    for( int i = 0; i < m_nCategoryCount; i++ )
        m_aCategories[i].nAppCount = 0;
    
    // Count apps in each category
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( m_pItems[i].nCategoryIndex >= 0 && m_pItems[i].nCategoryIndex < m_nCategoryCount )
        {
            m_aCategories[m_pItems[i].nCategoryIndex].nAppCount++;
            m_aCategories[0].nAppCount++; // "All Apps" gets everything
        }
    }
    
    char szDebug[256];
    sprintf( szDebug, "Categories: %d total\n", m_nCategoryCount );
    OutputDebugString( szDebug );
    for( int i = 0; i < m_nCategoryCount; i++ )
    {
        sprintf( szDebug, "  %s: %d apps\n", m_aCategories[i].szName, m_aCategories[i].nAppCount );
        OutputDebugString( szDebug );
    }
}

//-----------------------------------------------------------------------------
// LoadCatalogFromFile - Load and parse store.json
//-----------------------------------------------------------------------------
BOOL Store::LoadCatalogFromFile( const char* filename )
{
    // Read entire file into memory
    FILE* f = fopen( filename, "rb" );
    if( !f )
    {
        OutputDebugString( "Failed to open catalog file\n" );
        return FALSE;
    }
    
    fseek( f, 0, SEEK_END );
    long fileSize = ftell( f );
    fseek( f, 0, SEEK_SET );
    
    char* jsonData = new char[fileSize + 1];
    fread( jsonData, 1, fileSize, f );
    jsonData[fileSize] = '\0';
    fclose( f );
    
    OutputDebugString( "Loaded catalog file\n" );
    
    // Find "apps" array
    const char* appsStart = strstr( jsonData, "\"apps\"" );
    if( !appsStart )
    {
        delete[] jsonData;
        return FALSE;
    }
    
    // Find opening bracket of apps array
    const char* arrayStart = strchr( appsStart, '[' );
    if( !arrayStart )
    {
        delete[] jsonData;
        return FALSE;
    }
    
    // Count apps
    int appCount = 0;
    const char* p = arrayStart;
    while( *p && appCount < 100 )
    {
        if( *p == '{' )
        {
            // Find the closing brace
            const char* objStart = p;
            const char* objEnd = strchr( p + 1, '}' );
            if( !objEnd ) break;
            
            appCount++;
            p = objEnd + 1;
        }
        else
        {
            p++;
        }
    }
    
    OutputDebugString( "Found apps in catalog\n" );
    
    // Allocate items
    m_nItemCount = appCount;
    m_pItems = new StoreItem[m_nItemCount];
    
    // Parse each app
    p = arrayStart;
    int itemIndex = 0;
    
    while( *p && itemIndex < appCount )
    {
        if( *p == '{' )
        {
            const char* objStart = p;
            const char* objEnd = strchr( p + 1, '}' );
            if( !objEnd ) break;
            
            // Extract this object
            int objLen = objEnd - objStart + 1;
            char* objData = new char[objLen + 1];
            memcpy( objData, objStart, objLen );
            objData[objLen] = '\0';
            
            // Parse fields
            StoreItem* item = &m_pItems[itemIndex];
            
            strcpy( item->szName, FindJSONValue( objData, "name" ) );
            strcpy( item->szAuthor, FindJSONValue( objData, "author" ) );
            strcpy( item->szVersion, FindJSONValue( objData, "version" ) );
            strcpy( item->szDescription, FindJSONValue( objData, "description" ) );
            
            item->dwSize = atoi( FindJSONValue( objData, "size" ) );
            item->nCategoryIndex = GetOrCreateCategory( FindJSONValue( objData, "category" ) );
            item->eState = (StoreItem::State)atoi( FindJSONValue( objData, "state" ) );
            item->pIcon = NULL;
            
            delete[] objData;
            itemIndex++;
            p = objEnd + 1;
        }
        else
        {
            p++;
        }
    }
    
    delete[] jsonData;
    
    // Build category list and count apps
    BuildCategoryList();
    
    char szDebug[256];
    sprintf( szDebug, "Loaded %d apps from catalog\n", m_nItemCount );
    OutputDebugString( szDebug );
    
    return TRUE;
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

    // Initialize font using bundled font files
    if( FAILED( m_Font.Create( "D:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        OutputDebugString( "Warning: Could not load Arial_16.xpr font file.\n" );
    }

    // Initialize gamepads
    XBInput_CreateGamepads( &m_pGamepads );

    // Load app catalog from JSON file
    if( !LoadCatalogFromFile( "D:\\store.json" ) )
    {
        OutputDebugString( "Failed to load store.json, using fallback data\n" );
        
        // Fallback: Create minimal sample data if JSON fails
        m_nItemCount = 3;
        m_pItems = new StoreItem[m_nItemCount];
        
        int gamesIdx = GetOrCreateCategory("games");
        int mediaIdx = GetOrCreateCategory("media");
        int toolsIdx = GetOrCreateCategory("tools");
        
        sprintf( m_pItems[0].szName, "Doom 64" );
        sprintf( m_pItems[0].szDescription, "Classic FPS" );
        sprintf( m_pItems[0].szVersion, "v2.1" );
        sprintf( m_pItems[0].szAuthor, "id Software" );
        m_pItems[0].dwSize = 5 * 1024 * 1024;
        m_pItems[0].eState = StoreItem::STATE_INSTALLED;
        m_pItems[0].nCategoryIndex = gamesIdx;
        m_pItems[0].pIcon = NULL;
        
        sprintf( m_pItems[1].szName, "XBMC" );
        sprintf( m_pItems[1].szDescription, "Media Center" );
        sprintf( m_pItems[1].szVersion, "v3.5" );
        sprintf( m_pItems[1].szAuthor, "XBMC Team" );
        m_pItems[1].dwSize = 20 * 1024 * 1024;
        m_pItems[1].eState = StoreItem::STATE_NOT_DOWNLOADED;
        m_pItems[1].nCategoryIndex = mediaIdx;
        m_pItems[1].pIcon = NULL;
        
        sprintf( m_pItems[2].szName, "FTP Server" );
        sprintf( m_pItems[2].szDescription, "File Transfer" );
        sprintf( m_pItems[2].szVersion, "v3.2" );
        sprintf( m_pItems[2].szAuthor, "XBDev" );
        m_pItems[2].dwSize = 512 * 1024;
        m_pItems[2].eState = StoreItem::STATE_DOWNLOADED;
        m_pItems[2].nCategoryIndex = toolsIdx;
        m_pItems[2].pIcon = NULL;
        
        BuildCategoryList();
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Update - Handle input and logic
//-----------------------------------------------------------------------------
void Store::Update()
{
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
    
    for( int i = 0; i < m_nCategoryCount; i++ )
    {
        DWORD bgColor = (i == m_nSelectedCategory) ? COLOR_SIDEBAR_HOVER : COLOR_SIDEBAR;
        
        // Highlight selected category
        if( i == m_nSelectedCategory )
        {
            DrawRect( pd3dDevice, 0, itemY, m_fSidebarWidth, itemH, bgColor );
        }
        
        // Category name with count
        char szText[64];
        sprintf( szText, "%s (%d)", m_aCategories[i].szName, m_aCategories[i].nAppCount );
        DrawText( pd3dDevice, szText, 50.0f, itemY + 15.0f, COLOR_WHITE );
        
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
    if( m_nSelectedCategory >= 0 && m_nSelectedCategory < m_nCategoryCount )
        categoryName = m_aCategories[m_nSelectedCategory].szName;
    
    DrawText( pd3dDevice, categoryName, m_fSidebarWidth + 20.0f, 20.0f, COLOR_WHITE );
    
    // Build filtered indices array for current category
    m_nFilteredCount = 0;
    for( int i = 0; i < m_nItemCount && m_nFilteredCount < 100; i++ )
    {
        // "All Apps" (index 0) shows everything
        if( m_nSelectedCategory == 0 || m_pItems[i].nCategoryIndex == m_nSelectedCategory )
        {
            m_aFilteredIndices[m_nFilteredCount++] = i;
        }
    }
    
    // Clamp selection to valid range
    if( m_nSelectedItem >= m_nFilteredCount )
        m_nSelectedItem = m_nFilteredCount > 0 ? m_nFilteredCount - 1 : 0;
    
    // Draw grid of app cards
    int visibleItems = m_nGridRows * m_nGridCols;
    int page = m_nSelectedItem / visibleItems;
    int startIndex = page * visibleItems;
    
    for( int row = 0; row < m_nGridRows; row++ )
    {
        for( int col = 0; col < m_nGridCols; col++ )
        {
            int gridIndex = startIndex + (row * m_nGridCols + col);
            if( gridIndex >= m_nFilteredCount )
                break;
                
            // Get actual item index from filtered array
            int actualItemIndex = m_aFilteredIndices[gridIndex];
            
            float x = m_fGridStartX + col * (m_fCardWidth + 20.0f);
            float y = m_fGridStartY + row * (m_fCardHeight + 20.0f);
            
            BOOL bSelected = (gridIndex == m_nSelectedItem);
            DrawAppCard( pd3dDevice, &m_pItems[actualItemIndex], x, y, bSelected );
        }
    }

    // Draw bottom instructions bar
    DrawRect( pd3dDevice, m_fSidebarWidth, m_fScreenHeight - 50.0f, 
              m_fScreenWidth - m_fSidebarWidth, 50.0f, COLOR_SECONDARY );
    
    char szInstructions[128];
    sprintf( szInstructions, "A: Details  B: Exit  LT/RT: Category  (%d apps)", m_nFilteredCount );
    DrawText( pd3dDevice, szInstructions, m_fSidebarWidth + 20.0f, m_fScreenHeight - 30.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// RenderItemDetails - Option 2: Bottom action bar with state-based buttons
//-----------------------------------------------------------------------------
void Store::RenderItemDetails( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Get actual item from filtered indices
    if( m_nSelectedItem < 0 || m_nSelectedItem >= m_nFilteredCount )
        return;
        
    int actualItemIndex = m_aFilteredIndices[m_nSelectedItem];
    if( actualItemIndex < 0 || actualItemIndex >= m_nItemCount )
        return;

    StoreItem* pItem = &m_pItems[actualItemIndex];

    // Calculate layout - narrower right sidebar (30%)
    float sidebarW = m_fScreenWidth * 0.30f;
    if( sidebarW < 200.0f ) sidebarW = 200.0f;
    if( sidebarW > 280.0f ) sidebarW = 280.0f;
    
    float contentW = m_fScreenWidth - sidebarW;
    float actionBarH = 70.0f;
    
    // Left content area - Screenshot and description
    DrawRect( pd3dDevice, 0, 0, contentW, m_fScreenHeight, COLOR_BG );
    
    // App title and author at top
    DrawText( pd3dDevice, pItem->szName, 20.0f, 20.0f, COLOR_WHITE );
    DrawText( pd3dDevice, pItem->szAuthor, 20.0f, 40.0f, COLOR_TEXT_GRAY );
    
    // Large screenshot area
    float screenshotY = 70.0f;
    float screenshotH = m_fScreenHeight * 0.55f;
    DrawRect( pd3dDevice, 20.0f, screenshotY, contentW - 40.0f, screenshotH, COLOR_CARD_BG );
    
    // Description below screenshot
    float descY = screenshotY + screenshotH + 20.0f;
    DrawText( pd3dDevice, "Description:", 20.0f, descY, COLOR_TEXT_GRAY );
    DrawText( pd3dDevice, pItem->szDescription, 20.0f, descY + 25.0f, COLOR_WHITE );

    // Right sidebar - Metadata (narrower)
    float sidebarX = contentW;
    DrawRect( pd3dDevice, sidebarX, 0, sidebarW, m_fScreenHeight - actionBarH, COLOR_PRIMARY );
    
    // Metadata in sidebar
    float metaY = 20.0f;
    
    DrawText( pd3dDevice, "Title:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, pItem->szName, sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    DrawText( pd3dDevice, "Author:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, pItem->szAuthor, sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    DrawText( pd3dDevice, "Version:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, pItem->szVersion, sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    char szTemp[128];
    DrawText( pd3dDevice, "Size:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    sprintf( szTemp, "%.1f MB", pItem->dwSize / (1024.0f * 1024.0f) );
    DrawText( pd3dDevice, szTemp, sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    // Status indicator
    const char* statusText;
    DWORD statusColor;
    switch( pItem->eState )
    {
        case StoreItem::STATE_INSTALLED:
            statusText = "INSTALLED";
            statusColor = COLOR_SUCCESS;
            break;
        case StoreItem::STATE_DOWNLOADED:
            statusText = "DOWNLOADED";
            statusColor = COLOR_DOWNLOAD;
            break;
        case StoreItem::STATE_UPDATE_AVAILABLE:
            statusText = "UPDATE AVAILABLE";
            statusColor = 0xFFFF9800; // Orange
            break;
        default:
            statusText = "NOT DOWNLOADED";
            statusColor = COLOR_TEXT_GRAY;
            break;
    }
    DrawText( pd3dDevice, "Status:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, statusText, sidebarX + 15.0f, metaY, statusColor );

    // Bottom action bar (spans full width)
    float actionBarY = m_fScreenHeight - actionBarH;
    DrawRect( pd3dDevice, 0, actionBarY, m_fScreenWidth, actionBarH, COLOR_SECONDARY );
    
    // Buttons based on state - horizontal layout
    float btnY = actionBarY + 15.0f;
    float btnH = 40.0f;
    float btnSpacing = 20.0f;
    float btnStartX = 40.0f;
    
    switch( pItem->eState )
    {
        case StoreItem::STATE_NOT_DOWNLOADED:
        {
            // [Download] [Cancel]
            float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing) / 2.0f;
            
            DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_DOWNLOAD );
            DrawText( pd3dDevice, "(A) Download", btnStartX + 20.0f, btnY + 12.0f, COLOR_WHITE );
            
            DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, COLOR_CARD_BG );
            DrawText( pd3dDevice, "(B) Cancel", btnStartX + btnW + btnSpacing + 20.0f, btnY + 12.0f, COLOR_WHITE );
            break;
        }
        
        case StoreItem::STATE_DOWNLOADED:
        {
            // [Install] [Delete] [Cancel]
            float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing * 2) / 3.0f;
            
            DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_SUCCESS );
            DrawText( pd3dDevice, "(A) Install", btnStartX + 15.0f, btnY + 12.0f, COLOR_WHITE );
            
            DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, 0xFF9E9E9E );
            DrawText( pd3dDevice, "(X) Delete", btnStartX + btnW + btnSpacing + 15.0f, btnY + 12.0f, COLOR_WHITE );
            
            DrawRect( pd3dDevice, btnStartX + (btnW + btnSpacing) * 2, btnY, btnW, btnH, COLOR_CARD_BG );
            DrawText( pd3dDevice, "(B) Cancel", btnStartX + (btnW + btnSpacing) * 2 + 15.0f, btnY + 12.0f, COLOR_WHITE );
            break;
        }
        
        case StoreItem::STATE_INSTALLED:
        {
            // [Launch] [Uninstall] [Cancel]
            float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing * 2) / 3.0f;
            
            DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_SUCCESS );
            DrawText( pd3dDevice, "(A) Launch", btnStartX + 15.0f, btnY + 12.0f, COLOR_WHITE );
            
            DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, 0xFFD32F2F );
            DrawText( pd3dDevice, "(X) Uninstall", btnStartX + btnW + btnSpacing + 10.0f, btnY + 12.0f, COLOR_WHITE );
            
            DrawRect( pd3dDevice, btnStartX + (btnW + btnSpacing) * 2, btnY, btnW, btnH, COLOR_CARD_BG );
            DrawText( pd3dDevice, "(B) Cancel", btnStartX + (btnW + btnSpacing) * 2 + 15.0f, btnY + 12.0f, COLOR_WHITE );
            break;
        }
        
        case StoreItem::STATE_UPDATE_AVAILABLE:
        {
            // [Update] [Launch] [Cancel]
            float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing * 2) / 3.0f;
            
            DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, 0xFFFF9800 ); // Orange
            DrawText( pd3dDevice, "(A) Update", btnStartX + 15.0f, btnY + 12.0f, COLOR_WHITE );
            
            DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, COLOR_SUCCESS );
            DrawText( pd3dDevice, "(Y) Launch", btnStartX + btnW + btnSpacing + 15.0f, btnY + 12.0f, COLOR_WHITE );
            
            DrawRect( pd3dDevice, btnStartX + (btnW + btnSpacing) * 2, btnY, btnW, btnH, COLOR_CARD_BG );
            DrawText( pd3dDevice, "(B) Cancel", btnStartX + (btnW + btnSpacing) * 2 + 15.0f, btnY + 12.0f, COLOR_WHITE );
            break;
        }
    }
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
    
    // App name from filtered index
    if( m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
    {
        int actualItemIndex = m_aFilteredIndices[m_nSelectedItem];
        if( actualItemIndex >= 0 && actualItemIndex < m_nItemCount )
        {
            DrawText( pd3dDevice, m_pItems[actualItemIndex].szName, panelX + 20.0f, panelY + 20.0f, COLOR_WHITE );
        }
    }
    
    // Progress bar
    float barY = panelY + 80.0f;
    float barW = panelW - 40.0f;
    DrawRect( pd3dDevice, panelX + 20.0f, barY, barW, 30.0f, COLOR_SECONDARY );
    DrawRect( pd3dDevice, panelX + 22.0f, barY + 2.0f, (barW - 4.0f) * 0.5f, 26.0f, COLOR_DOWNLOAD );
    
    // Progress text
    DrawText( pd3dDevice, "50% Complete", panelX + 20.0f, barY + 50.0f, COLOR_WHITE );
    DrawText( pd3dDevice, "2048 KB / 4096 KB", panelX + 20.0f, barY + 70.0f, COLOR_TEXT_GRAY );
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
            
            char szDebug[256];
            sprintf( szDebug, "Detected resolution: %.0fx%.0f\n", m_fScreenWidth, m_fScreenHeight );
            OutputDebugString( szDebug );
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
    
    char szDebug[256];
    sprintf( szDebug, "Layout: %dx%d grid, cards %.0fx%.0f, sidebar %.0f\n", 
             m_nGridCols, m_nGridRows, m_fCardWidth, m_fCardHeight, m_fSidebarWidth );
    OutputDebugString( szDebug );
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
    // Card background
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
    
    // Status badge (top-right corner) - varies by state
    DWORD badgeColor;
    switch( pItem->eState )
    {
        case StoreItem::STATE_INSTALLED:
            badgeColor = COLOR_SUCCESS; // Green checkmark
            break;
        case StoreItem::STATE_DOWNLOADED:
            badgeColor = COLOR_DOWNLOAD; // Blue - ready to install
            break;
        case StoreItem::STATE_UPDATE_AVAILABLE:
            badgeColor = 0xFFFF9800; // Orange - update available
            break;
        default:
            badgeColor = COLOR_TEXT_GRAY; // Gray - not downloaded
            break;
    }
    DrawRect( pd3dDevice, x + m_fCardWidth - 35, y + 10, 25, 25, badgeColor );

    // App name below thumbnail
    DrawText( pd3dDevice, pItem->szName, x + 10, y + thumbSize + 15, COLOR_WHITE );
    
    // Author name (only if there's room)
    if( m_fCardHeight > 180.0f )
    {
        DrawText( pd3dDevice, pItem->szAuthor, x + 10, y + thumbSize + 35, COLOR_TEXT_GRAY );
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
            if( m_nSelectedCategory < 0 ) m_nSelectedCategory = m_nCategoryCount - 1;
            m_nSelectedItem = 0; // Reset selection
        }
        
        // Right trigger - next category
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
        {
            m_nSelectedCategory++;
            if( m_nSelectedCategory >= m_nCategoryCount ) m_nSelectedCategory = 0;
            m_nSelectedItem = 0; // Reset selection
        }
        
        // Navigate grid with D-pad
        if( wPressed & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            if( m_nSelectedItem % m_nGridCols > 0 )
                m_nSelectedItem--;
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            if( m_nSelectedItem % m_nGridCols < m_nGridCols - 1 )
                m_nSelectedItem++;
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_UP )
        {
            if( m_nSelectedItem >= m_nGridCols )
                m_nSelectedItem -= m_nGridCols;
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_nSelectedItem += m_nGridCols;
            // Clamp to valid range based on filtered items
            // (simplified - you'd want to count filtered items properly)
        }

        // A button - select item
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        {
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
            int actualItemIndex = m_aFilteredIndices[m_nSelectedItem];
            
            // B button - back to grid
            if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
            {
                m_CurrentState = UI_MAIN_GRID;
            }

            // A button - action based on state
            if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
            {
                switch( m_pItems[actualItemIndex].eState )
                {
                    case StoreItem::STATE_NOT_DOWNLOADED:
                        // Start download
                        m_CurrentState = UI_DOWNLOADING;
                        // TODO: Start actual download
                        break;
                        
                    case StoreItem::STATE_DOWNLOADED:
                        // Install the app
                        m_pItems[actualItemIndex].eState = StoreItem::STATE_INSTALLED;
                        // TODO: Start actual install
                        break;
                        
                    case StoreItem::STATE_INSTALLED:
                        // Launch the app
                        // TODO: Launch the XBE
                        break;
                        
                    case StoreItem::STATE_UPDATE_AVAILABLE:
                        // Download update
                        m_CurrentState = UI_DOWNLOADING;
                        // TODO: Start update download
                        break;
                }
            }
            
            // X button - secondary action (delete/uninstall)
            if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
            {
                if( m_pItems[actualItemIndex].eState == StoreItem::STATE_DOWNLOADED )
                {
                    // Delete downloaded file
                    m_pItems[actualItemIndex].eState = StoreItem::STATE_NOT_DOWNLOADED;
                    // TODO: Delete actual file
                }
                else if( m_pItems[actualItemIndex].eState == StoreItem::STATE_INSTALLED )
                {
                    // Uninstall app
                    m_pItems[actualItemIndex].eState = StoreItem::STATE_NOT_DOWNLOADED;
                    // TODO: Uninstall actual app
                }
            }
        }
    }
    else if( m_CurrentState == UI_DOWNLOADING )
    {
        // TODO: Handle download completion
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