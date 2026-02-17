//=============================================================================
// Context.cpp
//=============================================================================

#include "Context.h"
#include "Scenes/SceneManager.h"

static void* s_pDevice = NULL;
static SceneManager* s_pSceneManager = NULL;
static int s_nScreenWidth = 640;
static int s_nScreenHeight = 480;

void Context::SetDevice( void* pDevice )
{
    s_pDevice = pDevice;
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
