//=============================================================================
// Context.cpp
//=============================================================================

#include "Context.h"
#include "Defines.h"
#include "Scenes/SceneManager.h"

namespace {
    D3DDevice* mD3dDevice = nullptr;
    static SceneManager* s_pSceneManager = nullptr;
    static int32_t s_nScreenWidth = 640;
    static int32_t s_nScreenHeight = 480;
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

void Context::SetScreenSize( int32_t width, int32_t height )
{
    s_nScreenWidth = width;
    s_nScreenHeight = height;
}

SceneManager* Context::GetSceneManager()
{
    return s_pSceneManager;
}

float Context::GetScreenWidth()
{
    return (float)s_nScreenWidth;
}

float Context::GetScreenHeight()
{
    return (float)s_nScreenHeight;
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