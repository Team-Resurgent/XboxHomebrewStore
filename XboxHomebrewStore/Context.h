#pragma once

#include "Main.h"

class SceneManager;

class Context
{
public:
    static void SetD3dDevice(D3DDevice* d3dDevice);
    static D3DDevice* GetD3dDevice();

    static void SetDevice( void* pDevice );
    static void SetSceneManager( SceneManager* pMgr );
    static void SetScreenSize( int32_t width, int32_t height );
    static SceneManager* GetSceneManager();
    static float GetScreenWidth();
    static float GetScreenHeight();
    static int32_t GetGridCols();
    static int32_t GetGridRows();
    static int32_t GetGridCells();
};