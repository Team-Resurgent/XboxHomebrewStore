//=============================================================================
// Context.cpp
//=============================================================================

#include "Context.h"
#include "Defines.h"
#include "Scenes/SceneManager.h"

namespace {
    D3DDevice* mD3dDevice = nullptr;
    static SceneManager* s_pSceneManager = nullptr;
    static int32_t s_actualWidth = 640;
    static int32_t s_actualHeight = 480;
    static int32_t s_logicalWidth = 640;
    static int32_t s_logicalHeight = 480;
    static float s_safeLeft = 0.0f;
    static float s_safeTop = 0.0f;
    static float s_safeRight = 0.0f;
    static float s_safeBottom = 0.0f;
}

void Context::SetD3dDevice(D3DDevice* d3dDevice)
{
    mD3dDevice = d3dDevice;
}

D3DDevice* Context::GetD3dDevice()
{
    return mD3dDevice;
}

void Context::SetSceneManager( SceneManager* pMgr )
{
    s_pSceneManager = pMgr;
}

void Context::SetActualSize( int32_t width, int32_t height )
{
    s_actualWidth = width;
    s_actualHeight = height;
}

void Context::SetLogicalSize( int32_t width, int32_t height )
{
    s_logicalWidth = width > 0 ? width : 640;
    s_logicalHeight = height > 0 ? height : 480;
}

void Context::SetSafeAreaInsets( float left, float top, float right, float bottom )
{
    s_safeLeft = left >= 0.0f ? left : 0.0f;
    s_safeTop = top >= 0.0f ? top : 0.0f;
    s_safeRight = right >= 0.0f ? right : 0.0f;
    s_safeBottom = bottom >= 0.0f ? bottom : 0.0f;
}

SceneManager* Context::GetSceneManager()
{
    return s_pSceneManager;
}

float Context::GetScreenWidth()
{
    return (float)s_logicalWidth;
}

float Context::GetScreenHeight()
{
    return (float)s_logicalHeight;
}

float Context::GetActualScreenWidth()
{
    return (float)s_actualWidth;
}

float Context::GetActualScreenHeight()
{
    return (float)s_actualHeight;
}

float Context::GetScaleX()
{
    return s_logicalWidth > 0 ? (float)s_actualWidth / (float)s_logicalWidth : 1.0f;
}

float Context::GetScaleY()
{
    return s_logicalHeight > 0 ? (float)s_actualHeight / (float)s_logicalHeight : 1.0f;
}

float Context::GetSafeAreaLeft()
{
    return s_safeLeft;
}

float Context::GetSafeAreaTop()
{
    return s_safeTop;
}

float Context::GetSafeAreaRight()
{
    return s_safeRight;
}

float Context::GetSafeAreaBottom()
{
    return s_safeBottom;
}

float Context::GetSafeAreaX()
{
    return s_safeLeft;
}

float Context::GetSafeAreaY()
{
    return s_safeTop;
}

float Context::GetSafeAreaWidth()
{
    float w = (float)s_logicalWidth - s_safeLeft - s_safeRight;
    return w > 0.0f ? w : 0.0f;
}

float Context::GetSafeAreaHeight()
{
    float h = (float)s_logicalHeight - s_safeTop - s_safeBottom;
    return h > 0.0f ? h : 0.0f;
}

int32_t Context::GetGridCols()
{
    float gridWidth = GetScreenWidth() - ASSET_SIDEBAR_WIDTH; 
    return (int32_t)((gridWidth + CARD_GAP) / (ASSET_CARD_WIDTH + CARD_GAP));
}

int32_t Context::GetGridRows()
{
    float gridHeight = GetScreenHeight() - (ASSET_HEADER_HEIGHT + ASSET_FOOTER_HEIGHT);
    return (int32_t)((gridHeight + CARD_GAP) / (ASSET_CARD_HEIGHT + CARD_GAP));
}

int32_t Context::GetGridCells()
{
    return GetGridCols() * GetGridRows();
}