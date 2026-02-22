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
    static void SetScreenSize( int width, int height );
    static SceneManager* GetSceneManager();
    static float GetScreenWidth();
    static float GetScreenHeight();
    static int GetGridCols();
    static int GetGridRows();
    static int GetGridCells();
};