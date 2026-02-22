#include "VersionScene.h"
#include "VersionDetailsScene.h"
#include "SceneManager.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\Math.h"
#include "..\String.h"
#include "..\InputManager.h"
#include "..\TextureHelper.h"

VersionScene::VersionScene(const StoreVersions& storeVersions)
{
    mSelectedIndex = 0;
    mStoreVersions = storeVersions;

    mSideBarFocused = true;
    mHighlightedVersionIndex = 0;
    mVersionIndex = 0;
}

void VersionScene::Render()
{
    Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0, Context::GetScreenWidth(), Context::GetScreenHeight());
    
    RenderHeader();
    RenderFooter();
    RenderVersionSidebar();
    RenderListView();
}

void VersionScene::RenderHeader()
{
    Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0, Context::GetScreenWidth(), ASSET_HEADER_HEIGHT);
    Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
    Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386, 16, 12, ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
}

void VersionScene::RenderFooter()
{
    int32_t footerY = Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0, footerY, Context::GetScreenWidth(), ASSET_FOOTER_HEIGHT);

    Drawing::DrawTexturedRect(TextureHelper::GetControllerIcon("StickLeft"), 0xffffffff, 16, footerY + 10, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
    Font::DrawText(FONT_NORMAL, "Hide A: Details B: Exit LT/RT: Category D-pad: Move", COLOR_WHITE, 52, footerY + 12);
}

void VersionScene::RenderVersionSidebar()
{
    int32_t sidebarHeight = (Context::GetScreenHeight() - ASSET_SIDEBAR_Y) - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0, ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH, sidebarHeight);

    Font::DrawText(FONT_NORMAL, "Versions...", COLOR_WHITE, 16, ASSET_SIDEBAR_Y + 16);

    uint32_t versionCount = mStoreVersions.versions.size();

    uint32_t maxItems = (sidebarHeight - 64) / 44;
    uint32_t start = 0;
    if (versionCount >= maxItems) {
        start = Math::ClampInt32(mHighlightedVersionIndex - (maxItems / 2), 0, versionCount - maxItems);
    }

    uint32_t itemCount = Math::MinUint32(start + maxItems, versionCount) - start;

    for (uint32_t pass = 0; pass < 2; pass++) 
    {
        int32_t y = ASSET_SIDEBAR_Y + 64;

        if (pass == 1) {
            Drawing::BeginStencil(16, (float)ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH - 32.0f, (float)sidebarHeight);
        }

        for (uint32_t i = 0; i < itemCount; i++)
        {
            uint32_t index = start + i;

            StoreVersion* storeVersion = &mStoreVersions.versions[index];

            bool highlighted = index == mHighlightedVersionIndex;
            bool focused = mSideBarFocused && highlighted;

            if (pass == 0)
            {
                if (highlighted || focused)
                {
                    Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(), focused ? COLOR_FOCUS_HIGHLIGHT : COLOR_HIGHLIGHT, 0, y - 32, ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
                }
            }
            else
            {
                if (focused == true) {
                    Font::DrawTextScrolling(FONT_NORMAL, storeVersion->version, COLOR_WHITE, 16, y, ASSET_SIDEBAR_WIDTH - 64, &storeVersion->versionScrollState);
                } else {
                    Font::DrawText(FONT_NORMAL, storeVersion->version, COLOR_WHITE, 16, y);
                }
            }

            y += 44;
        }

        if (pass == 1) {
            Drawing::EndStencil();
        }
    }
}

//typedef struct
//{
//    uint32_t size;
//    std::string changeLog;
//} StoreVersion;
//
//typedef struct
//{
//    std::string name;
//    std::string author;
//    std::string description;
//} StoreVersions;

void VersionScene::RenderListView()
{
    int gridX = ASSET_SIDEBAR_WIDTH;
    int gridY = ASSET_SIDEBAR_Y;
    int gridWidth = Context::GetScreenWidth() - ASSET_SIDEBAR_WIDTH; 
    int gridHeight = Context::GetScreenHeight() - (ASSET_HEADER_HEIGHT + ASSET_FOOTER_HEIGHT);

    Font::DrawText(FONT_NORMAL, "Name:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.name, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Author:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.author, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Description:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.description, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;

    StoreVersion* storeVersion = &mStoreVersions.versions[mHighlightedVersionIndex];

    Font::DrawText(FONT_NORMAL, "Version:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->version, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Title ID:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->titleId, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Release Date:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->releaseDate, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Region:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->region, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Change Log:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->changeLog, COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Size:", COLOR_WHITE, 200, gridY);
    Font::DrawText(FONT_NORMAL, String::Format("%i", storeVersion->size), COLOR_TEXT_GRAY, 350, gridY);
    gridY += 30;
    
    /*int w = Context::GetScreenWidth();
    int h = Context::GetScreenHeight();
    float fW = (float)w;
    float fH = (float)h;

    float sidebarW = fW * 0.30f;
    if( sidebarW < 200.0f ) sidebarW = 200.0f;
    if( sidebarW > 280.0f ) sidebarW = 280.0f;
    float contentW = fW - sidebarW;
    float actionBarH = 70.0f;

    Font::DrawText(FONT_NORMAL, mStoreVersions.name, COLOR_WHITE, 200, 60 );
    Font::DrawText(FONT_NORMAL, mStoreVersions.author, (uint32_t)COLOR_TEXT_GRAY, 200, 85 );

    float screenshotY = 110.0f;
    float screenshotH = fH * 0.30f;
    if (mStoreVersions.screenshot )
    {
        Drawing::DrawTexturedRect(mStoreVersions.screenshot, 0xFFFFFFFF, (int)200, (int)screenshotY, ASSET_SCREENSHOT_WIDTH, ASSET_SCREENSHOT_HEIGHT);
    } else {
        Drawing::DrawTexturedRect(TextureHelper::GetScreenshotRef(), 0xFFFFFFFF, (int)200, (int)screenshotY, ASSET_SCREENSHOT_WIDTH, ASSET_SCREENSHOT_HEIGHT);
    }

  Drawing::DrawTexturedRect(TextureHelper::GetCoverRef(), 0xFFFFFFFF, 36 + ASSET_SCREENSHOT_WIDTH, screenshotY, 144, 204);


    float contentY = screenshotY + screenshotH + 20.0f;
    Font::DrawText(FONT_NORMAL, "Description:", (uint32_t)COLOR_TEXT_GRAY, (int)200, (int)contentY );
    contentY += 25.0f;
    Font::DrawText(FONT_NORMAL, mStoreVersions.description, (uint32_t)COLOR_WHITE, (int)200, (int)contentY );
    contentY += 50.0f;

    if( mStoreVersions.versions.size() > 0 )
    {
        Font::DrawText(FONT_NORMAL, "Available Versions (UP/DOWN to browse, A to view):", (uint32_t)COLOR_TEXT_GRAY, (int)20.0f, (int)contentY );
        contentY += 30.0f;
        int maxVisible = 3;
        int scrollOffset = 0;
        if( mSelectedIndex >= scrollOffset + maxVisible )
            scrollOffset = mSelectedIndex - maxVisible + 1;
        if( mSelectedIndex < scrollOffset )
            scrollOffset = mSelectedIndex;
        if( scrollOffset > 0 )
        {
            Font::DrawText(FONT_NORMAL, "^ More above", (uint32_t)COLOR_TEXT_GRAY, 30, (int)contentY );
            contentY += 20.0f;
        }
        for( int i = 0; i < maxVisible && (i + scrollOffset) < (int)mStoreVersions.versions.size(); i++ )
        {
            int vIdx = i + scrollOffset;
            const StoreVersion& v = mStoreVersions.versions[vIdx];
            BOOL bSelected = (vIdx == mSelectedIndex);
            float itemH = 55.0f;
            DWORD bgColor = bSelected ? COLOR_PRIMARY : COLOR_CARD_BG;
            Drawing::DrawFilledRect( (uint32_t)bgColor, (int)200, (int)contentY, (int)(contentW - 240.0f), (int)itemH );
            std::string szVer = String::Format( "v%s", v.version.c_str() );
            Font::DrawText(FONT_NORMAL, szVer.c_str(), COLOR_WHITE, 30, (int)(contentY + 8.0f) );
            if( !v.releaseDate.empty() )
                Font::DrawText(FONT_NORMAL, v.releaseDate.c_str(), (uint32_t)COLOR_TEXT_GRAY, 30, (int)(contentY + 28.0f) );
            std::string szSize = String::Format( "%.1f MB", v.size / (1024.0f * 1024.0f) );
            Font::DrawText(FONT_NORMAL, szSize.c_str(), COLOR_WHITE, (int)(contentW - 120.0f), (int)(contentY + 18.0f) );
            contentY += itemH + 5.0f;
        }
        if( scrollOffset + maxVisible < (int)mStoreVersions.versions.size() )
            Font::DrawText(FONT_NORMAL, "v More below", (uint32_t)COLOR_TEXT_GRAY, 30, (int)(contentY + 5.0f) );
    }*/

    //float sidebarX = contentW;
    //Drawing::DrawFilledRect( (uint32_t)COLOR_PRIMARY, (int)sidebarX, 0, (int)sidebarW, (int)(fH - actionBarH) );
    //float metaY = 20.0f;
    //Font::DrawText(FONT_NORMAL, "Title:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    //metaY += 20.0f;
    //Font::DrawText(FONT_NORMAL, mStoreVersions.name, COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    //metaY += 40.0f;
    //Font::DrawText(FONT_NORMAL, "Author:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    //metaY += 20.0f;
    //Font::DrawText(FONT_NORMAL, mStoreVersions.author, COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    //metaY += 40.0f;
    //Font::DrawText(FONT_NORMAL, String::Format( "%u version(s)", (unsigned)mStoreVersions.versions.size() ).c_str(), (uint32_t)COLOR_TEXT_GRAY, (int)(sidebarX + 15.0f), (int)metaY );

    //float actionBarY = fH - actionBarH;
    //Drawing::DrawFilledRect( (uint32_t)COLOR_SECONDARY, 0, (int)actionBarY, w, (int)actionBarH );
    //Font::DrawText(FONT_NORMAL, "(A) View Version  (B) Back to Grid", COLOR_WHITE, 40, (int)(actionBarY + 25.0f) );
}

void VersionScene::Update()
{
    if(InputManager::ControllerPressed(ControllerB, -1))
    {
        SceneManager* pMgr = Context::GetSceneManager();
        if( pMgr )
            pMgr->PopScene();
        return;
    }

    int n = (int)mStoreVersions.versions.size();
    if( n > 0 )
    {
        if(InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            mSelectedIndex--;
            if( mSelectedIndex < 0 )
                mSelectedIndex = n - 1;
        }
        if(InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            mSelectedIndex++;
            if( mSelectedIndex >= n )
                mSelectedIndex = 0;
        }
        if(InputManager::ControllerPressed(ControllerA, -1))
        {
            SceneManager* pMgr = Context::GetSceneManager();
            /*if( pMgr )
                pMgr->PushScene( new VersionDetailsScene( m_info, m_selectedIndex ) );*/
        }
    }
}
