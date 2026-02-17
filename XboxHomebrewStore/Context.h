//=============================================================================
// Context.h - Global app context (device, scene manager, screen size)
//=============================================================================

#ifndef CONTEXT_H
#define CONTEXT_H

class SceneManager;

class Context
{
public:
    static void SetDevice( void* pDevice );
    static void SetSceneManager( SceneManager* pMgr );
    static void SetScreenSize( int width, int height );
    static SceneManager* GetSceneManager();
    static int GetScreenWidth();
    static int GetScreenHeight();
};

#endif // CONTEXT_H
