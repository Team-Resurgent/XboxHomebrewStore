//=============================================================================
// Context.cpp
//=============================================================================

#include "Context.h"
#include "Defines.h"
#include "Scenes/SceneManager.h"

namespace {
    D3DDevice* mD3dDevice = NULL;
    static SceneManager* s_pSceneManager = NULL;
    static int s_nScreenWidth = 640;
    static int s_nScreenHeight = 480;
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

void Context::SetScreenSize( int width, int height )
{
    s_nScreenWidth = width;
    s_nScreenHeight = height;
}

SceneManager* Context::GetSceneManager()
{
    return s_pSceneManager;
}

int Context::GetScreenWidth()
{
    return s_nScreenWidth;
}

int Context::GetScreenHeight()
{
    return s_nScreenHeight;
}

int Context::GetGridCols()
{
    int gridWidth = GetScreenWidth() - ASSET_SIDEBAR_WIDTH; 
    return (gridWidth + CARD_GAP) / (ASSET_CARD_WIDTH + CARD_GAP);
}

int Context::GetGridRows()
{
    int gridHeight = GetScreenHeight() - (ASSET_HEADER_HEIGHT + ASSET_FOOTER_HEIGHT);
    return (gridHeight + CARD_GAP) / (ASSET_CARD_HEIGHT + CARD_GAP);
}

int Context::GetGridCells()
{
    return GetGridCols() * GetGridRows();
}