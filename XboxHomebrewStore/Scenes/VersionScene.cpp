#include "VersionScene.h"
#include "VersionDetailsScene.h"
#include "SceneManager.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\String.h"
#include "..\InputManager.h"

VersionScene::VersionScene( const SelectedAppInfo& info )
    : m_info( info )
    , m_selectedIndex( 0 )
{
    if( m_info.versions.empty() )
        return;
    if( m_selectedIndex >= (int)m_info.versions.size() )
        m_selectedIndex = (int)m_info.versions.size() - 1;
    if( m_selectedIndex < 0 )
        m_selectedIndex = 0;
}

void VersionScene::Render( LPDIRECT3DDEVICE8 pd3dDevice )
{
    (void)pd3dDevice;
    RenderListView( pd3dDevice );
}

void VersionScene::RenderListView( LPDIRECT3DDEVICE8 pd3dDevice )
{
    (void)pd3dDevice;
    int w = Context::GetScreenWidth();
    int h = Context::GetScreenHeight();
    float fW = (float)w;
    float fH = (float)h;

    float sidebarW = fW * 0.30f;
    if( sidebarW < 200.0f ) sidebarW = 200.0f;
    if( sidebarW > 280.0f ) sidebarW = 280.0f;
    float contentW = fW - sidebarW;
    float actionBarH = 70.0f;

    Drawing::DrawFilledRect( COLOR_BG, 0, 0, (int)contentW, h );
    Font::DrawText( m_info.appName.c_str(), COLOR_WHITE, 20, 20 );
    Font::DrawText( m_info.author.c_str(), (uint32_t)COLOR_TEXT_GRAY, 20, 45 );

    float screenshotY = 70.0f;
    float screenshotH = fH * 0.30f;
    Drawing::DrawFilledRect( (uint32_t)COLOR_CARD_BG, (int)20.0f, (int)screenshotY, (int)(contentW - 40.0f), (int)screenshotH );
    if( m_info.pScreenshot )
        Drawing::DrawTexturedRect( (D3DTexture*)m_info.pScreenshot, 0xFFFFFFFF, (int)20.0f, (int)screenshotY, (int)(contentW - 40.0f), (int)screenshotH );

    float contentY = screenshotY + screenshotH + 20.0f;
    Font::DrawText( "Description:", (uint32_t)COLOR_TEXT_GRAY, (int)20.0f, (int)contentY );
    contentY += 25.0f;
    Font::DrawText( m_info.description.c_str(), (uint32_t)COLOR_WHITE, (int)20.0f, (int)contentY );
    contentY += 50.0f;

    if( m_info.versions.size() > 1 )
    {
        Font::DrawText( "Available Versions (UP/DOWN to browse, A to view):", (uint32_t)COLOR_TEXT_GRAY, (int)20.0f, (int)contentY );
        contentY += 30.0f;
        int maxVisible = 3;
        int scrollOffset = 0;
        if( m_selectedIndex >= scrollOffset + maxVisible )
            scrollOffset = m_selectedIndex - maxVisible + 1;
        if( m_selectedIndex < scrollOffset )
            scrollOffset = m_selectedIndex;
        if( scrollOffset > 0 )
        {
            Font::DrawText( "^ More above", (uint32_t)COLOR_TEXT_GRAY, 30, (int)contentY );
            contentY += 20.0f;
        }
        for( int i = 0; i < maxVisible && (i + scrollOffset) < (int)m_info.versions.size(); i++ )
        {
            int vIdx = i + scrollOffset;
            const VersionInfo& v = m_info.versions[vIdx];
            BOOL bSelected = (vIdx == m_selectedIndex);
            float itemH = 55.0f;
            DWORD bgColor = bSelected ? COLOR_PRIMARY : COLOR_CARD_BG;
            Drawing::DrawFilledRect( (uint32_t)bgColor, (int)20.0f, (int)contentY, (int)(contentW - 40.0f), (int)itemH );
            std::string szVer = String::Format( "v%s", v.version.c_str() );
            Font::DrawText( szVer.c_str(), COLOR_WHITE, 30, (int)(contentY + 8.0f) );
            if( !v.releaseDate.empty() )
                Font::DrawText( v.releaseDate.c_str(), (uint32_t)COLOR_TEXT_GRAY, 30, (int)(contentY + 28.0f) );
            std::string szSize = String::Format( "%.1f MB", v.size / (1024.0f * 1024.0f) );
            Font::DrawText( szSize.c_str(), COLOR_WHITE, (int)(contentW - 120.0f), (int)(contentY + 18.0f) );
            contentY += itemH + 5.0f;
        }
        if( scrollOffset + maxVisible < (int)m_info.versions.size() )
            Font::DrawText( "v More below", (uint32_t)COLOR_TEXT_GRAY, 30, (int)(contentY + 5.0f) );
    }

    float sidebarX = contentW;
    Drawing::DrawFilledRect( (uint32_t)COLOR_PRIMARY, (int)sidebarX, 0, (int)sidebarW, (int)(fH - actionBarH) );
    float metaY = 20.0f;
    Font::DrawText( "Title:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 20.0f;
    Font::DrawText( m_info.appName.c_str(), COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 40.0f;
    Font::DrawText( "Author:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 20.0f;
    Font::DrawText( m_info.author.c_str(), COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 40.0f;
    Font::DrawText( String::Format( "%u version(s)", (unsigned)m_info.versions.size() ).c_str(), (uint32_t)COLOR_TEXT_GRAY, (int)(sidebarX + 15.0f), (int)metaY );

    float actionBarY = fH - actionBarH;
    Drawing::DrawFilledRect( (uint32_t)COLOR_SECONDARY, 0, (int)actionBarY, w, (int)actionBarH );
    Font::DrawText( "(A) View Version  (B) Back to Grid", COLOR_WHITE, 40, (int)(actionBarY + 25.0f) );
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

    int n = (int)m_info.versions.size();
    if( n > 0 )
    {
        if(InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            m_selectedIndex--;
            if( m_selectedIndex < 0 )
                m_selectedIndex = n - 1;
        }
        if(InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            m_selectedIndex++;
            if( m_selectedIndex >= n )
                m_selectedIndex = 0;
        }
        if(InputManager::ControllerPressed(ControllerA, -1))
        {
            SceneManager* pMgr = Context::GetSceneManager();
            if( pMgr )
                pMgr->PushScene( new VersionDetailsScene( m_info, m_selectedIndex ) );
        }
    }
}
