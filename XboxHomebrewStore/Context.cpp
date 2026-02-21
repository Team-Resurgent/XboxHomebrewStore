//=============================================================================
// Context.cpp
//=============================================================================

#include "Context.h"
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
