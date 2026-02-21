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
#include "..\StoreManager.h"

StoreScene::StoreScene()
: m_CurrentState( UI_MAIN_GRID )
    , m_bFocusOnSidebar( TRUE )
    , m_nSelectedCol( 0 )
    , m_nSelectedRow( 0 )
    , m_nSelectedItem( 0 )
    , m_nScrollOffset( 0 )
    , m_nSelectedCategory( 0 )
    , m_fSidebarWidth( 220.0f )
    , m_fGridStartX( 0.0f )
    , m_fGridStartY( 60.0f )
    , m_fCardWidth( 160.0f )
    , m_fCardHeight( 220.0f )
    , m_nGridCols( 4 )
    , m_nGridRows( 2 )
{
    mSideBarFocused = true;
    mHighlightedCategoryIndex = 0;
    mStoreIndex = 0;
}

StoreScene::~StoreScene()
{
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
    m_nGridCols = STORE_GRID_COLS;
    m_nGridRows = STORE_GRID_ROWS;
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

    Drawing::DrawTexturedRect(TextureHelper::GetControllerIcon("StickLeft"), 0xffffffff, 16, footerY + 10, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
    Font::DrawText(FONT_NORMAL, "Hide A: Details B: Exit LT/RT: Category D-pad: Move", COLOR_WHITE, 52, footerY + 12);
}

void StoreScene::RenderCategorySidebar()
{
    int32_t sidebarHeight = (Context::GetScreenHeight() - ASSET_SIDEBAR_Y) - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0, ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH, sidebarHeight);

    int32_t y = ASSET_SIDEBAR_Y + 30;

    uint32_t categoryCount = StoreManager::GetCategoryCount();
    for (uint32_t i = 0; i < categoryCount; i++)
    {
        CategoryItem* categoryItem = StoreManager::GetCategory(i);

        bool highlighted = i == mHighlightedCategoryIndex;
        bool focused = mSideBarFocused && highlighted;
        if (highlighted || focused)
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(), focused ? COLOR_FOCUS_HIGHLIGHT : COLOR_HIGHLIGHT, 0, y - 32, ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
        }

        bool activated = i == StoreManager::GetCategoryIndex();
        if (mSideBarFocused == true)
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(categoryItem->category), activated ? COLOR_FOCUS_HIGHLIGHT : 0xffffffff, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }
        else
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(categoryItem->category), activated ? COLOR_HIGHLIGHT : 0xffffffff, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }

        Font::DrawText(FONT_NORMAL, categoryItem->category.c_str(), COLOR_WHITE, 48, y);
        y += 44;
    }
}

void StoreScene::DrawStoreItem(StoreItem* storeItem, int x, int y, bool selected)
{
    uint32_t bgColor = selected ? (uint32_t)COLOR_CARD_SEL : (uint32_t)COLOR_CARD_BG;
    //Drawing::DrawFilledRect(bgColor, (int)x, (int)y, (int)w, (int)h );

    Drawing::DrawTexturedRect(TextureHelper::GetCard(), 0xFFFFFFFF, x, y, ASSET_CARD_WIDTH, ASSET_CARD_HEIGHT);
    
    if (selected == true) {
        Drawing::DrawTexturedRect(TextureHelper::GetCardHighlight(), mSideBarFocused ? COLOR_HIGHLIGHT : COLOR_FOCUS_HIGHLIGHT, x - 3, y - 3, ASSET_CARD_HIGHLIGHT_WIDTH, ASSET_CARD_HIGHLIGHT_HEIGHT);
    }

    int iconW = ASSET_CARD_WIDTH - 18;
    int iconH = ASSET_CARD_HEIGHT - 60;
    int iconX = x + 9;
    int iconY = y + 9;
    if( iconW > 0 && iconH > 0 )
    {
        Drawing::DrawFilledRect(COLOR_SECONDARY, iconX, iconY, iconW, iconH);
        if (storeItem->cover != NULL) {
            Drawing::DrawTexturedRect(storeItem->cover, 0xFFFFFFFF, iconX, iconY, iconW, iconH);
        }
    }

    Font::DrawText(FONT_NORMAL, storeItem->name.c_str(), COLOR_WHITE, x + 8, y + iconH + 14);
    Font::DrawText(FONT_NORMAL, storeItem->author.c_str(), COLOR_TEXT_GRAY, x + 8, y + iconH + 32);
}

void StoreScene::RenderMainGrid()
{
    int count = StoreManager::GetWindowStoreItemCount();
    if( count == 0 )
    {
        Font::DrawText(FONT_NORMAL, "No apps in this category.", (uint32_t)COLOR_TEXT_GRAY, (int)m_fGridStartX, (int)m_fGridStartY );
        return;
    }

    int gridX = ASSET_SIDEBAR_WIDTH;
    int gridY = ASSET_HEADER_HEIGHT;
    int gridWidth = Context::GetScreenWidth() - ASSET_SIDEBAR_WIDTH; 
    int gridHeight = Context::GetScreenHeight() - (ASSET_HEADER_HEIGHT + ASSET_FOOTER_HEIGHT);

    int cardWidth = ASSET_CARD_WIDTH;
    int cardHeight = ASSET_CARD_HEIGHT;
    int storeItemsWidth = (STORE_GRID_COLS * (cardWidth + CARD_GAP)) - CARD_GAP;
    int storeItemsHeight = (STORE_GRID_ROWS * (cardHeight + CARD_GAP)) - CARD_GAP;

    int cardX = gridX + ((gridWidth - storeItemsWidth) / 2);
    int cardY = gridY + ((gridHeight - storeItemsHeight) / 2);;


    uint32_t totalSlots = STORE_GRID_COLS * STORE_GRID_ROWS;
    uint32_t windowCount = StoreManager::GetWindowStoreItemCount();
    uint32_t slotsInView = Math::MinInt32(totalSlots, windowCount);
    for (uint32_t currentSlot = 0; currentSlot < slotsInView; currentSlot++ )
    {
        int row = currentSlot / STORE_GRID_COLS;
        int col = currentSlot % STORE_GRID_COLS;
        int x = cardX + col * ( cardWidth + CARD_GAP);
        int y = cardY + row * ( cardHeight + CARD_GAP);
        StoreItem* storeItem = StoreManager::GetWindowStoreItem(currentSlot);
        DrawStoreItem(storeItem, x, y, currentSlot == (mStoreIndex - StoreManager::GetWindowStoreItemOffset()));
    }

    uint32_t subindex = 0;
    std::string pageStr = String::Format("Item %d of %d", mStoreIndex + 1, StoreManager::GetSelectedCategoryTotal());
    Font::DrawText(FONT_NORMAL, pageStr.c_str(), (uint32_t)COLOR_TEXT_GRAY, 600, Context::GetScreenHeight() - 30);
}

void StoreScene::RenderDownloading( LPDIRECT3DDEVICE8 pd3dDevice )
{
    ///*(void)pd3dDevice;
    //int w = (int)m_fScreenWidth;
    //int h = (int)m_fScreenHeight;
    //Drawing::DrawFilledRect( 0xE0000000, 0, 0, w, h );
    //float cx = m_fScreenWidth * 0.5f;
    //float cy = m_fScreenHeight * 0.5f;
    //Font::DrawText(FONT_NORMAL, m_pStore->GetDownloadAppName().c_str(), COLOR_WHITE, (int)( cx - 100.0f ), (int)( cy - 60.0f ) );
    //Font::DrawText(FONT_NORMAL, "Downloading...", (uint32_t)COLOR_TEXT_GRAY, (int)( cx - 60.0f ), (int)( cy - 30.0f ) );
    //uint32_t now = m_pStore->GetDownloadNow();
    //uint32_t total = m_pStore->GetDownloadTotal();
    //if( total > 0 )
    //{
    //    std::string prog = String::Format( "%u / %u KB", now / 1024, total / 1024 );
    //    Font::DrawText(FONT_NORMAL, prog.c_str(), COLOR_WHITE, (int)( cx - 40.0f ), (int)( cy + 10.0f ) );
    //    float barW = 400.0f;
    //    float barX = cx - barW * 0.5f;
    //    Drawing::DrawFilledRect( (uint32_t)COLOR_SECONDARY, (int)barX, (int)( cy + 40.0f ), (int)barW, 24 );
    //    int filled = (int)( barW * (float)now / (float)total );
    //    if( filled > 0 )
    //        Drawing::DrawFilledRect( (uint32_t)COLOR_PRIMARY, (int)barX, (int)( cy + 40.0f ), filled, 24 );
    //}
    //Font::DrawText(FONT_NORMAL, "(B) Cancel", (uint32_t)COLOR_TEXT_GRAY, (int)( cx - 40.0f ), (int)( cy + 80.0f )*/ );
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
    CalculateLayout();

    Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0, Context::GetScreenWidth(), Context::GetScreenHeight());
    switch( m_CurrentState )
    {
        case UI_MAIN_GRID:
            RenderHeader();
            RenderFooter();
            RenderCategorySidebar();
            RenderMainGrid();
            break;
        case UI_DOWNLOADING:
            RenderMainGrid();
            RenderDownloading( pd3dDevice );
            break;
        case UI_SETTINGS:
            RenderSettings( pd3dDevice );
            break;
        default:
            RenderHeader();
            RenderFooter();
            RenderCategorySidebar();
            RenderMainGrid();
            break;
    }
}

void StoreScene::HandleInput()
{
    if( m_CurrentState == UI_DOWNLOADING )
    {
        if (InputManager::ControllerPressed(ControllerB, -1))
        {
            //if( m_pStore->GetDownloadThread() != NULL && !m_pStore->GetDownloadDone() )
            //    m_pStore->SetDownloadCancelRequested( true );
            //else
            //{
            //    m_pStore->CloseDownloadThread();
            //    m_CurrentState = UI_MAIN_GRID;
            //}
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
    int numCategories = StoreManager::GetCategoryCount();
    int count = StoreManager::GetCategory(0)->count;
    int totalSlots = m_nGridCols * m_nGridRows;
    int baseIndex = m_nScrollOffset;
    int visibleCount = totalSlots;
    if( baseIndex + visibleCount > count ) visibleCount = count - baseIndex;
    if( visibleCount < 0 ) visibleCount = 0;
    //m_pStore->SetVisibleRange( baseIndex, visibleCount );


    if (mSideBarFocused)
    {
        if (InputManager::ControllerPressed( ControllerDpadUp, -1 ) )
        {
            mHighlightedCategoryIndex = mHighlightedCategoryIndex > 0 ? mHighlightedCategoryIndex - 1 : StoreManager::GetCategoryCount() - 1;
        }
        if(InputManager::ControllerPressed( ControllerDpadDown, -1 ) )
        {
            mHighlightedCategoryIndex = mHighlightedCategoryIndex < StoreManager::GetCategoryCount() - 1 ? mHighlightedCategoryIndex + 1 : 0;
        }
    }

    if( m_bFocusOnSidebar )
    {
        if( InputManager::ControllerPressed( ControllerDpadRight, -1 ) )
        {
            m_bFocusOnSidebar = FALSE;
            m_nSelectedCol = 0;
            m_nSelectedRow = 0;
        }
        else if ( numCategories > 0 )
        {
            bool categoryChanged = false;
     
            if( InputManager::ControllerPressed( ControllerA, -1 ) )
                categoryChanged = true;
            if( categoryChanged )
            {
                //std::string filter = StoreManager::GetCategoryFilter(m_nSelectedCategory);
                //m_pStore->LoadAppsPage( 1, filter.c_str(), m_nSelectedCategory );
                m_nScrollOffset = 0;
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
                //std::string filter = StoreManager::GetCategoryFilter(m_nSelectedCategory);
                //m_pStore->LoadAppsPage( 1, filter.c_str(), m_nSelectedCategory );
                m_nScrollOffset = 0;
                m_nSelectedCol = 0;
                m_nSelectedRow = 0;
            }
            if( InputManager::ControllerPressed( ControllerRTrigger, -1 ) )
            {
                m_nSelectedCategory++;
                if( m_nSelectedCategory >= numCategories ) m_nSelectedCategory = 0;
                //std::string filter = StoreManager::GetCategoryFilter(m_nSelectedCategory);
                //m_pStore->LoadAppsPage( 1, filter.c_str(), m_nSelectedCategory );
                m_nScrollOffset = 0;
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
                if( m_nSelectedRow > 0 )
                {
                    m_nSelectedRow--;
                }
                else
                {
                    if( m_nScrollOffset > 0 )
                    {
                        m_nScrollOffset -= m_nGridCols;
                        if( m_nScrollOffset < 0 ) m_nScrollOffset = 0;
                        m_nSelectedRow = m_nGridRows - 1;
                        m_nSelectedCol = 0;
                    }
                    else
                    {
                        //std::string filter = ( m_nSelectedCategory == 0 || numCategories == 0 ) ? "" : StoreManager::GetCategory(m_nSelectedCategory)->category;
                        //if( m_pStore->HasPreviousPage() && m_pStore->GoToPrevPage( filter.c_str() ) )
                        //{
                        //    int newCount = m_pStore->GetFilteredCount();
                        //    m_nScrollOffset = ( newCount > totalSlots ) ? ( newCount - totalSlots ) : 0;
                        //    m_nSelectedRow = m_nGridRows - 1;
                        //    m_nSelectedCol = 0;
                        //}
                    }
                }
            }
            if( InputManager::ControllerPressed( ControllerDpadDown, -1 ) )
            {
                if( m_nSelectedRow < m_nGridRows - 1 )
                {
                    int itemIndex = m_nScrollOffset + ( m_nSelectedRow + 1 ) * m_nGridCols + m_nSelectedCol;
                    if( itemIndex < count )
                        m_nSelectedRow++;
                }
                else
                {
                    int nextRowStart = m_nScrollOffset + totalSlots;
                    if( nextRowStart < count )
                    {
                        m_nScrollOffset += m_nGridCols;
                        m_nSelectedRow = 0;
                        int firstRowItems = count - m_nScrollOffset;
                        if( firstRowItems > m_nGridCols ) firstRowItems = m_nGridCols;
                        if( firstRowItems > 0 && m_nSelectedCol >= firstRowItems ) m_nSelectedCol = firstRowItems - 1;
                    }
      /*              else if( m_pStore->HasNextPage() && m_pStore->AppendNextPage() )
                    {
                        m_nScrollOffset += m_nGridCols;
                        m_nSelectedRow = 0;
                        count = m_pStore->GetFilteredCount();
                        int firstRowItems = count - m_nScrollOffset;
                        if( firstRowItems > m_nGridCols ) firstRowItems = m_nGridCols;
                        if( firstRowItems > 0 && m_nSelectedCol >= firstRowItems ) m_nSelectedCol = firstRowItems - 1;
                    }*/
                }
            }
    /*        count = m_pStore->GetFilteredCount();
            {
                int maxScroll = ( count > totalSlots ) ? ( count - totalSlots ) : 0;
                if( m_nScrollOffset > maxScroll ) m_nScrollOffset = maxScroll;
                if( m_nScrollOffset < 0 ) m_nScrollOffset = 0;
            }*/
            if( InputManager::ControllerPressed( ControllerA, -1 ) )
            {
     /*           int totalSlotsA = m_nGridCols * m_nGridRows;
                int baseIdx = m_nScrollOffset;
                m_nSelectedItem = baseIdx + m_nSelectedRow * m_nGridCols + m_nSelectedCol;
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
                    info.appId = pItem->id;
                    info.appName = pItem->name;
                    info.author = pItem->author;
                    info.description = pItem->description;
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
                }*/
            }
        }
    }

    if( InputManager::ControllerPressed( ControllerY, -1 ) )
        m_CurrentState = UI_SETTINGS;
}

void StoreScene::Update()
{
    //if( !m_pStore )
    //    return;
    //m_pStore->ProcessImageDownloader();
    //if( !m_pStore->GetDownloadDone() && ( !m_pStore->GetDownloadAppName().empty() || m_pStore->GetDownloadThread() != NULL ) )
    //    m_CurrentState = UI_DOWNLOADING;
    //if( m_CurrentState == UI_DOWNLOADING && m_pStore->GetDownloadThread() == NULL && !m_pStore->GetDownloadDone() )
    //    m_pStore->StartDownloadThread();
    //HandleInput();

    if (mSideBarFocused)
    {
        if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            mHighlightedCategoryIndex = mHighlightedCategoryIndex > 0 ? mHighlightedCategoryIndex - 1 : StoreManager::GetCategoryCount() - 1;
        }
        else if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            mHighlightedCategoryIndex = mHighlightedCategoryIndex < StoreManager::GetCategoryCount() - 1 ? mHighlightedCategoryIndex + 1 : 0;
        }
        else if (InputManager::ControllerPressed(ControllerA, -1))
        {
            bool needsUpdate = StoreManager::GetCategoryIndex() != mHighlightedCategoryIndex;
            if (needsUpdate == true) {
                StoreManager::SetCategoryIndex(mHighlightedCategoryIndex);
                mStoreIndex = 0;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadRight, -1))
        {
            mSideBarFocused = false;
        }
    }
    else
    {
        // need to check item count

        int col = mStoreIndex % STORE_GRID_COLS;
        int row = mStoreIndex / STORE_GRID_COLS;
        if (InputManager::ControllerPressed(ControllerDpadLeft, -1))
        {
            if (col == 0) {
                mSideBarFocused = true;
            } else {
                mStoreIndex--;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadRight, -1))
        {
            if (col < (STORE_GRID_COLS - 1) && mStoreIndex < (StoreManager::GetSelectedCategoryTotal() - 1)) {
                mStoreIndex++;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            if ((mStoreIndex - StoreManager::GetWindowStoreItemOffset()) >= STORE_GRID_COLS)
            {
                mStoreIndex -= STORE_GRID_COLS;
            }
            else if (StoreManager::HasPrevious())
            {
                StoreManager::LoadPrevious();
                mStoreIndex = mStoreIndex >= STORE_GRID_COLS ? mStoreIndex - STORE_GRID_COLS : 0;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            if (mStoreIndex < (StoreManager::GetWindowStoreItemOffset() + StoreManager::GetWindowStoreItemCount() - STORE_GRID_COLS))
            {
                mStoreIndex += STORE_GRID_COLS;
            }
            else if (StoreManager::HasNext())
            {
                StoreManager::LoadNext();
                mStoreIndex = Math::MinUint32(mStoreIndex + STORE_GRID_COLS, StoreManager::GetSelectedCategoryTotal() - 1);
            }
        }
    }

}
