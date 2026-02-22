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

void VersionDetailsScene::Render()
{
    float w = (float)Context::GetScreenWidth();
    float h = (float)Context::GetScreenHeight();
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

    Drawing::DrawFilledRect( COLOR_BG, 0.0f, 0.0f, contentW, h );
    std::string szTitle = String::Format( "%s v%s", m_info.appName.c_str(), v.version.c_str() );
    Font::DrawText(FONT_NORMAL, szTitle.c_str(), COLOR_WHITE, 20.0f, 20.0f );
    Font::DrawText(FONT_NORMAL, m_info.author.c_str(), (uint32_t)COLOR_TEXT_GRAY, 20.0f, 40.0f );

    float screenshotY = 70.0f;
    float screenshotH = fH * 0.45f;
    Drawing::DrawFilledRect( (uint32_t)COLOR_CARD_BG, 20.0f, screenshotY, contentW - 40.0f, screenshotH );
    if( m_info.pScreenshot )
        Drawing::DrawTexturedRect( (D3DTexture*)m_info.pScreenshot, 0xFFFFFFFF, 20.0f, screenshotY, (contentW - 40.0f), screenshotH );

    float descY = screenshotY + screenshotH + 20.0f;
    Font::DrawText(FONT_NORMAL, "Description:", (uint32_t)COLOR_TEXT_GRAY, 20.0f, descY );
    descY += 25.0f;
    Font::DrawText(FONT_NORMAL, m_info.description.c_str(), COLOR_WHITE, 20.0f, descY );
    descY += 50.0f;
    if( !v.changeLog.empty() && v.changeLog != "No changelog available" )
    {
        Font::DrawText(FONT_NORMAL, "What's New:", (uint32_t)COLOR_TEXT_GRAY, 20.0f, descY );
        descY += 25.0f;
        Font::DrawText(FONT_NORMAL, v.changeLog.c_str(), COLOR_WHITE, 20.0f, descY );
    }

    float sidebarX = contentW;
    Drawing::DrawFilledRect( (uint32_t)COLOR_PRIMARY, sidebarX, 0, sidebarW, (fH - actionBarH) );
    float metaY = 20.0f;
    Font::DrawText(FONT_NORMAL, "Version:", COLOR_WHITE, (sidebarX + 15.0f), metaY );
    metaY += 20.0f;
    Font::DrawText(FONT_NORMAL, v.version.c_str(), COLOR_WHITE, (sidebarX + 15.0f), metaY );
    metaY += 40.0f;
    if( !v.releaseDate.empty() )
    {
        Font::DrawText(FONT_NORMAL, "Released:", COLOR_WHITE, (sidebarX + 15.0f), metaY );
        metaY += 20.0f;
        Font::DrawText(FONT_NORMAL, v.releaseDate.c_str(), COLOR_WHITE, (sidebarX + 15.0f), metaY );
        metaY += 40.0f;
    }
    Font::DrawText(FONT_NORMAL, "Size:", COLOR_WHITE, (sidebarX + 15.0f), metaY );
    metaY += 20.0f;
    Font::DrawText(FONT_NORMAL, String::Format( "%.1f MB", v.size / (1024.0f * 1024.0f) ).c_str(), COLOR_WHITE, (sidebarX + 15.0f), metaY );
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
    Font::DrawText(FONT_NORMAL, "Status:", COLOR_WHITE, (sidebarX + 15.0f), metaY );
    metaY += 20.0f;
    Font::DrawText(FONT_NORMAL, statusText, statusColor, (sidebarX + 15.0f), metaY );

    float actionBarY = fH - actionBarH;
    Drawing::DrawFilledRect( (uint32_t)COLOR_SECONDARY, 0, actionBarY, w, actionBarH );
    float btnY = actionBarY + 15.0f;
    float btnH = 40.0f;
    float btnSpacing = 20.0f;
    float btnStartX = 40.0f;
    float btnW = (fW - btnStartX * 2 - btnSpacing) / 2.0f;
    Drawing::DrawFilledRect( (uint32_t)COLOR_DOWNLOAD, btnStartX, btnY, btnW, btnH );
    Font::DrawText(FONT_NORMAL, "(A) Download", COLOR_WHITE, (btnStartX + 20.0f), (btnY + 12.0f) );
    Drawing::DrawFilledRect( (uint32_t)COLOR_CARD_BG, (btnStartX + btnW + btnSpacing), btnY, btnW, btnH );
    Font::DrawText(FONT_NORMAL, "(B) Back", COLOR_WHITE, (btnStartX + btnW + btnSpacing + 40.0f), (btnY + 12.0f) );
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
