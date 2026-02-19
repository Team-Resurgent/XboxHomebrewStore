#include "StoreScene.h"
#include "SceneManager.h"
#include "VersionScene.h"

#include "..\Main.h"
#include "..\Math.h"
#include "..\Defines.h"
#include "..\Context.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\String.h"
#include "..\InputManager.h"

StoreScene::StoreScene( Store* pStore )
    : m_pStore( pStore )
    , m_CurrentState( UI_MAIN_GRID )
    , m_bFocusOnSidebar( FALSE )
    , m_nSelectedCol( 0 )
    , m_nSelectedRow( 0 )
    , m_nSelectedItem( 0 )
    , m_nSelectedCategory( 0 )
    , m_fScreenWidth( 1280.0f )
    , m_fScreenHeight( 720.0f )
    , m_fSidebarWidth( 220.0f )
    , m_fGridStartX( 0.0f )
    , m_fGridStartY( 60.0f )
    , m_fCardWidth( 160.0f )
    , m_fCardHeight( 220.0f )
    , m_nGridCols( 4 )
    , m_nGridRows( 2 )
    , m_bLayoutValid( FALSE )
{
}

StoreScene::~StoreScene()
{
}

void StoreScene::DetectResolution()
{
    int w = Context::GetScreenWidth();
    int h = Context::GetScreenHeight();
    m_fScreenWidth = (float)w;
    m_fScreenHeight = (float)h;
}

void StoreScene::CalculateLayout()
{
    m_fSidebarWidth = m_fScreenWidth * 0.22f;
    if( m_fSidebarWidth < 180.0f ) m_fSidebarWidth = 180.0f;
    if( m_fSidebarWidth > 260.0f ) m_fSidebarWidth = 260.0f;



    m_fGridStartX = m_fSidebarWidth + 20.0f;
    m_fGridStartY = 60.0f;
    m_fCardWidth = 160.0f;
    m_fCardHeight = 220.0f;
    float contentW = m_fScreenWidth - m_fSidebarWidth - 40.0f;
    float contentH = m_fScreenHeight - m_fGridStartY - 80.0f;
    m_nGridCols = (int)( ( contentW + CARD_GAP ) / ( m_fCardWidth + CARD_GAP ) );
    m_nGridRows = (int)( ( contentH + CARD_GAP ) / ( m_fCardHeight + CARD_GAP ) );
    if( m_nGridCols < 1 ) m_nGridCols = 1;
    if( m_nGridRows < 1 ) m_nGridRows = 1;
}

void StoreScene::EnsureLayout( LPDIRECT3DDEVICE8 pd3dDevice )
{
    (void)pd3dDevice;
    if( m_bLayoutValid )
        return;
    DetectResolution();
    CalculateLayout();
    m_bLayoutValid = TRUE;
}

void StoreScene::RenderHeader()
{
    Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0, Context::GetScreenWidth(), ASSET_HEADER_HEIGHT);
    Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
    Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386, 16, 12, ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
}

void StoreScene::RenderFooter()
{
    int32_t footerY = Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0, footerY, Context::GetScreenWidth(), ASSET_FOOTER_HEIGHT);
}

void StoreScene::RenderCategorySidebar()
{
    int32_t sidebarHeight = (Context::GetScreenHeight() - ASSET_SIDEBAR_Y) - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0, ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH, sidebarHeight);

    const std::vector<CategoryItem>& categories = m_pStore->GetCategories();

    int32_t y = ASSET_SIDEBAR_Y + 30;
    for (uint32_t i = 0; i < categories.size(); i++)
    {
        bool selected = i == m_nSelectedCategory;
        bool focused = m_bFocusOnSidebar && selected;

        if( focused )
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(), 0xffdf4088, 0, y - 32, ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(categories[i].category), 0xffdf4088, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }
        else if( selected )
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(), 0xff5d283f, 0, y - 32, ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(categories[i].category), 0xff5d283f, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }
        else
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(categories[i].category), 0xffffffff, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }

        Font::DrawText(FONT_NORMAL, categories[i].category.c_str(), COLOR_WHITE, 48, y);
        y += 44;
    }
}

void StoreScene::DrawAppCard( LPDIRECT3DDEVICE8 pd3dDevice, int itemIndex, float x, float y, float w, float h, BOOL selected )
{
    (void)pd3dDevice;
    StoreItem* pItems = m_pStore->GetItems();
    int* pFiltered = m_pStore->GetFilteredIndices();
    int count = m_pStore->GetFilteredCount();
    if( itemIndex < 0 || itemIndex >= count || !pItems || !pFiltered )
        return;
    StoreItem* pItem = &pItems[pFiltered[itemIndex]];
    DWORD bgColor = selected ? (uint32_t)COLOR_CARD_SEL : (uint32_t)COLOR_CARD_BG;
    Drawing::DrawFilledRect( bgColor, (int)x, (int)y, (int)w, (int)h );
    float iconW = w - 16.0f;
    float iconH = 140.0f;
    float iconX = x + 8.0f;
    float iconY = y + 8.0f;
    if( iconW > 0 && iconH > 0 )
    {
        Drawing::DrawFilledRect( (uint32_t)COLOR_SECONDARY, (int)iconX, (int)iconY, (int)iconW, (int)iconH );
        if( pItem->pIcon )
            Drawing::DrawTexturedRect( (D3DTexture*)pItem->pIcon, 0xFFFFFFFF, (int)iconX, (int)iconY, (int)iconW, (int)iconH );
    }
    Font::DrawText(FONT_NORMAL, pItem->app.name.c_str(), COLOR_WHITE, (int)( x + 8.0f ), (int)( y + iconH + 14.0f ) );
    Font::DrawText(FONT_NORMAL, pItem->app.author.c_str(), (uint32_t)COLOR_TEXT_GRAY, (int)( x + 8.0f ), (int)( y + iconH + 32.0f ) );
}

void StoreScene::RenderMainGrid( LPDIRECT3DDEVICE8 pd3dDevice )
{
    m_pStore->BuildFilteredIndices( m_nSelectedCategory );
    int count = m_pStore->GetFilteredCount();
    if( count <= 0 )
    {
        Font::DrawText(FONT_NORMAL, "No apps in this category.", (uint32_t)COLOR_TEXT_GRAY, (int)m_fGridStartX, (int)m_fGridStartY );
        return;
    }
    int selectedSlot = m_nSelectedRow * m_nGridCols + m_nSelectedCol;
    if( selectedSlot >= count ) selectedSlot = count - 1;
    if( selectedSlot < 0 ) selectedSlot = 0;
    m_nSelectedItem = selectedSlot;
    int totalSlots = m_nGridCols * m_nGridRows;
    for( int slot = 0; slot < totalSlots; slot++ )
    {
        if( slot >= count )
            break;
        int row = slot / m_nGridCols;
        int col = slot % m_nGridCols;
        float x = m_fGridStartX + col * ( m_fCardWidth + CARD_GAP );
        float y = m_fGridStartY + row * ( m_fCardHeight + CARD_GAP );
        BOOL selected = ( !m_bFocusOnSidebar && slot == selectedSlot );
        DrawAppCard( pd3dDevice, slot, x, y, m_fCardWidth, m_fCardHeight, selected );
    }
    float pageY = m_fScreenHeight - 50.0f;
    std::string pageStr = String::Format( "Page %d / %d  (%d items)", m_pStore->GetCurrentPage(), m_pStore->GetTotalPages(), m_pStore->GetTotalCount() );
    Font::DrawText(FONT_NORMAL, pageStr.c_str(), (uint32_t)COLOR_TEXT_GRAY, (int)m_fGridStartX, (int)pageY );
}

void StoreScene::RenderDownloading( LPDIRECT3DDEVICE8 pd3dDevice )
{
    (void)pd3dDevice;
    int w = (int)m_fScreenWidth;
    int h = (int)m_fScreenHeight;
    Drawing::DrawFilledRect( 0xE0000000, 0, 0, w, h );
    float cx = m_fScreenWidth * 0.5f;
    float cy = m_fScreenHeight * 0.5f;
    Font::DrawText(FONT_NORMAL, m_pStore->GetDownloadAppName().c_str(), COLOR_WHITE, (int)( cx - 100.0f ), (int)( cy - 60.0f ) );
    Font::DrawText(FONT_NORMAL, "Downloading...", (uint32_t)COLOR_TEXT_GRAY, (int)( cx - 60.0f ), (int)( cy - 30.0f ) );
    uint32_t now = m_pStore->GetDownloadNow();
    uint32_t total = m_pStore->GetDownloadTotal();
    if( total > 0 )
    {
        std::string prog = String::Format( "%u / %u KB", now / 1024, total / 1024 );
        Font::DrawText(FONT_NORMAL, prog.c_str(), COLOR_WHITE, (int)( cx - 40.0f ), (int)( cy + 10.0f ) );
        float barW = 400.0f;
        float barX = cx - barW * 0.5f;
        Drawing::DrawFilledRect( (uint32_t)COLOR_SECONDARY, (int)barX, (int)( cy + 40.0f ), (int)barW, 24 );
        int filled = (int)( barW * (float)now / (float)total );
        if( filled > 0 )
            Drawing::DrawFilledRect( (uint32_t)COLOR_PRIMARY, (int)barX, (int)( cy + 40.0f ), filled, 24 );
    }
    Font::DrawText(FONT_NORMAL, "(B) Cancel", (uint32_t)COLOR_TEXT_GRAY, (int)( cx - 40.0f ), (int)( cy + 80.0f ) );
}

void StoreScene::RenderSettings( LPDIRECT3DDEVICE8 pd3dDevice )
{
    (void)pd3dDevice;
    Drawing::DrawFilledRect( (uint32_t)COLOR_BG, 0, 0, (int)m_fScreenWidth, (int)m_fScreenHeight );
    Font::DrawText(FONT_NORMAL, "Settings", COLOR_WHITE, 40, 40 );
    Font::DrawText(FONT_NORMAL, "(B) Back to Store", (uint32_t)COLOR_TEXT_GRAY, 40, 100 );
}

void StoreScene::Render( LPDIRECT3DDEVICE8 pd3dDevice )
{
    if( !m_pStore || !pd3dDevice )
        return;
    EnsureLayout( pd3dDevice );

    Drawing::DrawTexturedRect( TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0, (int)m_fScreenWidth, (int)m_fScreenHeight );
    switch( m_CurrentState )
    {
        case UI_MAIN_GRID:
            RenderHeader();
            RenderFooter();
            RenderCategorySidebar();
            RenderMainGrid( pd3dDevice );
            break;
        case UI_DOWNLOADING:
            RenderMainGrid( pd3dDevice );
            RenderDownloading( pd3dDevice );
            break;
        case UI_SETTINGS:
            RenderSettings( pd3dDevice );
            break;
        default:
            RenderHeader();
            RenderFooter();
            RenderCategorySidebar();
            RenderMainGrid( pd3dDevice );
            break;
    }
}

void StoreScene::HandleInput()
{
    if( m_CurrentState == UI_DOWNLOADING )
    {
        if (InputManager::ControllerPressed(ControllerB, -1))
        {
            if( m_pStore->GetDownloadThread() != NULL && !m_pStore->GetDownloadDone() )
                m_pStore->SetDownloadCancelRequested( true );
            else
            {
                m_pStore->CloseDownloadThread();
                m_CurrentState = UI_MAIN_GRID;
            }
        }
        return;
    }

    if( m_CurrentState == UI_SETTINGS )
    {
        if(InputManager::ControllerPressed(ControllerB, -1))
        {
            m_CurrentState = UI_MAIN_GRID;
        }
        return;
    }

    // UI_MAIN_GRID
    const std::vector<CategoryItem>& cats = m_pStore->GetCategories();
    int numCategories = (int)cats.size();
    m_pStore->BuildFilteredIndices( m_nSelectedCategory );
    int count = m_pStore->GetFilteredCount();
    int totalSlots = m_nGridCols * m_nGridRows;

    if( m_bFocusOnSidebar )
    {
        if( InputManager::ControllerPressed( ControllerDpadRight, -1 ) )
        {
            m_bFocusOnSidebar = FALSE;
            m_nSelectedCol = 0;
            m_nSelectedRow = 0;
        }
        else if( numCategories > 0 )
        {
            if( InputManager::ControllerPressed( ControllerDpadUp, -1 ) )
            {
                m_nSelectedCategory--;
                if( m_nSelectedCategory < 0 ) m_nSelectedCategory = numCategories - 1;
                const char* filter = ( m_nSelectedCategory == 0 ) ? "" : cats[m_nSelectedCategory].category.c_str();
                m_pStore->LoadAppsPage( 1, filter, m_nSelectedCategory );
                m_nSelectedCol = 0;
                m_nSelectedRow = 0;
            }
            if( InputManager::ControllerPressed( ControllerDpadDown, -1 ) )
            {
                m_nSelectedCategory++;
                if( m_nSelectedCategory >= numCategories ) m_nSelectedCategory = 0;
                const char* filter = ( m_nSelectedCategory == 0 ) ? "" : cats[m_nSelectedCategory].category.c_str();
                m_pStore->LoadAppsPage( 1, filter, m_nSelectedCategory );
                m_nSelectedCol = 0;
                m_nSelectedRow = 0;
            }
        }
    }
    else
    {
        if( numCategories > 0 )
        {
            if( InputManager::ControllerPressed( ControllerLTrigger, -1 ) )
            {
                m_nSelectedCategory--;
                if( m_nSelectedCategory < 0 ) m_nSelectedCategory = numCategories - 1;
                const char* filter = ( m_nSelectedCategory == 0 ) ? "" : cats[m_nSelectedCategory].category.c_str();
                m_pStore->LoadAppsPage( 1, filter, m_nSelectedCategory );
                m_nSelectedCol = 0;
                m_nSelectedRow = 0;
            }
            if( InputManager::ControllerPressed( ControllerRTrigger, -1 ) )
            {
                m_nSelectedCategory++;
                if( m_nSelectedCategory >= numCategories ) m_nSelectedCategory = 0;
                const char* filter = ( m_nSelectedCategory == 0 ) ? "" : cats[m_nSelectedCategory].category.c_str();
                m_pStore->LoadAppsPage( 1, filter, m_nSelectedCategory );
                m_nSelectedCol = 0;
                m_nSelectedRow = 0;
            }
        }

        if( count > 0 && totalSlots > 0 )
        {
            if( InputManager::ControllerPressed( ControllerDpadLeft, -1 ) )
            {
                if( m_nSelectedCol > 0 )
                    m_nSelectedCol--;
                else
                    m_bFocusOnSidebar = TRUE;
            }
            if( InputManager::ControllerPressed( ControllerDpadRight, -1 ) )
            {
                int newCol = m_nSelectedCol + 1;
                if( newCol < m_nGridCols )
                {
                    int newSlot = m_nSelectedRow * m_nGridCols + newCol;
                    if( newSlot < count )
                        m_nSelectedCol = newCol;
                }
            }
            if( InputManager::ControllerPressed( ControllerDpadUp, -1 ) )
            {
                int page = m_pStore->GetCurrentPage();
                if( page > 1 )
                {
                    const char* filter = ( m_nSelectedCategory == 0 || numCategories == 0 ) ? "" : cats[m_nSelectedCategory].category.c_str();
                    m_pStore->LoadAppsPage( page - 1, filter, m_nSelectedCategory );
                    m_nSelectedCol = 0;
                    m_nSelectedRow = 0;
                }
            }
            if( InputManager::ControllerPressed( ControllerDpadDown, -1 ) )
            {
                int page = m_pStore->GetCurrentPage();
                int totalPages = m_pStore->GetTotalPages();
                if( page < totalPages )
                {
                    const char* filter = ( m_nSelectedCategory == 0 || numCategories == 0 ) ? "" : cats[m_nSelectedCategory].category.c_str();
                    m_pStore->LoadAppsPage( page + 1, filter, m_nSelectedCategory );
                    m_nSelectedCol = 0;
                    m_nSelectedRow = 0;
                }
            }
            if( InputManager::ControllerPressed( ControllerA, -1 ) )
            {
                m_nSelectedItem = m_nSelectedRow * m_nGridCols + m_nSelectedCol;
                if( m_nSelectedItem >= count ) m_nSelectedItem = count - 1;
                if( m_nSelectedItem < 0 ) m_nSelectedItem = 0;
                int* pFiltered = m_pStore->GetFilteredIndices();
                StoreItem* pItems = m_pStore->GetItems();
                if( pFiltered && pItems && m_nSelectedItem < count )
                {
                    StoreItem* pItem = &pItems[pFiltered[m_nSelectedItem]];
                    m_pStore->EnsureVersionsForItem( pItem );
                    m_pStore->EnsureScreenshotForItem( pItem );
                    m_pStore->MarkAppAsViewed( pItem->app.id.c_str() );
                    SelectedAppInfo info;
                    info.appId = pItem->app.id;
                    info.appName = pItem->app.name;
                    info.author = pItem->app.author;
                    info.description = pItem->app.description;
                    info.pScreenshot = pItem->pScreenshot;
                    for( size_t v = 0; v < pItem->versions.size(); v++ )
                    {
                        VersionInfo vi;
                        vi.id = pItem->versions[v].id;
                        vi.version = pItem->versions[v].version;
                        vi.size = pItem->versions[v].size;
                        vi.releaseDate = pItem->versions[v].releaseDate;
                        vi.changeLog = pItem->versions[v].changeLog;
                        vi.titleId = pItem->versions[v].titleId;
                        vi.region = pItem->versions[v].region;
                        vi.state = (int)m_pStore->GetDisplayState( pItem, (int)v );
                        info.versions.push_back( vi );
                    }
                    SceneManager* pMgr = Context::GetSceneManager();
                    if( pMgr )
                        pMgr->PushScene( new VersionScene( info ) );
                }
            }
        }
    }

    if( InputManager::ControllerPressed( ControllerY, -1 ) )
        m_CurrentState = UI_SETTINGS;
}

void StoreScene::Update()
{
    if( !m_pStore )
        return;
    m_pStore->ProcessImageDownloader();
    if( !m_pStore->GetDownloadDone() && ( !m_pStore->GetDownloadAppName().empty() || m_pStore->GetDownloadThread() != NULL ) )
        m_CurrentState = UI_DOWNLOADING;
    if( m_CurrentState == UI_DOWNLOADING && m_pStore->GetDownloadThread() == NULL && !m_pStore->GetDownloadDone() )
        m_pStore->StartDownloadThread();
    HandleInput();
}
