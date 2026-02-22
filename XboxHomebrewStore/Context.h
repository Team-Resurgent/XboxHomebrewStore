#pragma once

#include "Main.h"

class SceneManager;

/** Logical size = design resolution (e.g. 640x480). Actual = backbuffer size. Scale maps logical -> actual. */
class Context
{
public:
    static void SetD3dDevice(D3DDevice* d3dDevice);
    static D3DDevice* GetD3dDevice();

    static void SetDevice( void* pDevice );
    static void SetSceneManager( SceneManager* pMgr );
    static void SetActualSize( int32_t width, int32_t height );
    static void SetLogicalSize( int32_t width, int32_t height );
    static void SetSafeAreaInsets( float left, float top, float right, float bottom );
    static SceneManager* GetSceneManager();
    static float GetScreenWidth();
    static float GetScreenHeight();
    static float GetActualScreenWidth();
    static float GetActualScreenHeight();
    static float GetScaleX();
    static float GetScaleY();
    static float GetSafeAreaLeft();
    static float GetSafeAreaTop();
    static float GetSafeAreaRight();
    static float GetSafeAreaBottom();
    static float GetSafeAreaX();
    static float GetSafeAreaY();
    static float GetSafeAreaWidth();
    static float GetSafeAreaHeight();
    static int32_t GetGridCols();
    static int32_t GetGridRows();
    static int32_t GetGridCells();
};