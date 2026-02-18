#include "VersionDetailsScene.h"
#include "SceneManager.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\String.h"
#include "..\InputManager.h"

VersionDetailsScene::VersionDetailsScene( const SelectedAppInfo& info, int versionIndex )
    : m_info( info )
    , m_versionIndex( versionIndex )
{
    if( m_versionIndex < 0 || m_versionIndex >= (int)m_info.versions.size() )
        m_versionIndex = 0;
}

void VersionDetailsScene::Render( LPDIRECT3DDEVICE8 pd3dDevice )
{
    (void)pd3dDevice;
    int w = Context::GetScreenWidth();
    int h = Context::GetScreenHeight();
    float fW = (float)w;
    float fH = (float)h;

    if( m_versionIndex < 0 || m_versionIndex >= (int)m_info.versions.size() )
        return;
    const VersionInfo& v = m_info.versions[m_versionIndex];

    float sidebarW = fW * 0.30f;
    if( sidebarW < 200.0f ) sidebarW = 200.0f;
    if( sidebarW > 280.0f ) sidebarW = 280.0f;
    float contentW = fW - sidebarW;
    float actionBarH = 70.0f;

    Drawing::DrawFilledRect( COLOR_BG, 0, 0, (int)contentW, h );
    std::string szTitle = String::Format( "%s v%s", m_info.appName.c_str(), v.version.c_str() );
    Font::DrawText( szTitle.c_str(), COLOR_WHITE, 20, 20 );
    Font::DrawText( m_info.author.c_str(), (uint32_t)COLOR_TEXT_GRAY, 20, 40 );

    float screenshotY = 70.0f;
    float screenshotH = fH * 0.45f;
    Drawing::DrawFilledRect( (uint32_t)COLOR_CARD_BG, (int)20.0f, (int)screenshotY, (int)(contentW - 40.0f), (int)screenshotH );
    if( m_info.pScreenshot )
        Drawing::DrawTexturedRect( (D3DTexture*)m_info.pScreenshot, (int)20.0f, (int)screenshotY, (int)(contentW - 40.0f), (int)screenshotH );

    float descY = screenshotY + screenshotH + 20.0f;
    Font::DrawText( "Description:", (uint32_t)COLOR_TEXT_GRAY, 20, (int)descY );
    descY += 25.0f;
    Font::DrawText( m_info.description.c_str(), COLOR_WHITE, 20, (int)descY );
    descY += 50.0f;
    if( !v.changeLog.empty() && v.changeLog != "No changelog available" )
    {
        Font::DrawText( "What's New:", (uint32_t)COLOR_TEXT_GRAY, 20, (int)descY );
        descY += 25.0f;
        Font::DrawText( v.changeLog.c_str(), COLOR_WHITE, 20, (int)descY );
    }

    float sidebarX = contentW;
    Drawing::DrawFilledRect( (uint32_t)COLOR_PRIMARY, (int)sidebarX, 0, (int)sidebarW, (int)(fH - actionBarH) );
    float metaY = 20.0f;
    Font::DrawText( "Version:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 20.0f;
    Font::DrawText( v.version.c_str(), COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 40.0f;
    if( !v.releaseDate.empty() )
    {
        Font::DrawText( "Released:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
        metaY += 20.0f;
        Font::DrawText( v.releaseDate.c_str(), COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
        metaY += 40.0f;
    }
    Font::DrawText( "Size:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 20.0f;
    Font::DrawText( String::Format( "%.1f MB", v.size / (1024.0f * 1024.0f) ).c_str(), COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 40.0f;
    const char* statusText;
    DWORD statusColor;
    switch( v.state )
    {
        case 2: statusText = "INSTALLED"; statusColor = COLOR_SUCCESS; break;
        case 1: statusText = "DOWNLOADED"; statusColor = COLOR_DOWNLOAD; break;
        case 3: statusText = "UPDATE"; statusColor = 0xFFFF9800; break;
        default: statusText = "NOT DOWNLOADED"; statusColor = (DWORD)COLOR_TEXT_GRAY; break;
    }
    Font::DrawText( "Status:", COLOR_WHITE, (int)(sidebarX + 15.0f), (int)metaY );
    metaY += 20.0f;
    Font::DrawText( statusText, statusColor, (int)(sidebarX + 15.0f), (int)metaY );

    float actionBarY = fH - actionBarH;
    Drawing::DrawFilledRect( (uint32_t)COLOR_SECONDARY, 0, (int)actionBarY, w, (int)actionBarH );
    float btnY = actionBarY + 15.0f;
    float btnH = 40.0f;
    float btnSpacing = 20.0f;
    float btnStartX = 40.0f;
    float btnW = (fW - btnStartX * 2 - btnSpacing) / 2.0f;
    Drawing::DrawFilledRect( (uint32_t)COLOR_DOWNLOAD, (int)btnStartX, (int)btnY, (int)btnW, (int)btnH );
    Font::DrawText( "(A) Download", COLOR_WHITE, (int)(btnStartX + 20.0f), (int)(btnY + 12.0f) );
    Drawing::DrawFilledRect( (uint32_t)COLOR_CARD_BG, (int)(btnStartX + btnW + btnSpacing), (int)btnY, (int)btnW, (int)btnH );
    Font::DrawText( "(B) Back", COLOR_WHITE, (int)(btnStartX + btnW + btnSpacing + 40.0f), (int)(btnY + 12.0f) );
}

void VersionDetailsScene::Update()
{
    if(InputManager::ControllerPressed(ControllerB, -1))
    {
        SceneManager* pMgr = Context::GetSceneManager();
        if( pMgr )
            pMgr->PopScene();
    }
}
