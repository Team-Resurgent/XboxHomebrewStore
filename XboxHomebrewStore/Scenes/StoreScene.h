//=============================================================================
// StoreScene.h - Store UI (main grid, sidebar, downloading, settings)
//=============================================================================

#ifndef STORESCENE_H
#define STORESCENE_H

#include "..\Main.h"
#include "..\Store.h"
#include "Scene.h"

class StoreScene : public Scene
{
public:
    explicit StoreScene( Store* pStore );
    virtual ~StoreScene();
    virtual void Render( LPDIRECT3DDEVICE8 pd3dDevice );
    virtual void Update();

private:
    void EnsureLayout( LPDIRECT3DDEVICE8 pd3dDevice );
    void DetectResolution();
    void CalculateLayout();
    void HandleInput();
    void RenderCategorySidebar( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderMainGrid( LPDIRECT3DDEVICE8 pd3dDevice );
    void DrawAppCard( LPDIRECT3DDEVICE8 pd3dDevice, int itemIndex, float x, float y, float w, float h, BOOL selected );
    void RenderDownloading( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderSettings( LPDIRECT3DDEVICE8 pd3dDevice );

    Store* m_pStore;
    UIState m_CurrentState;
    int m_nSelectedItem;
    int m_nSelectedCategory;
    float m_fScreenWidth;
    float m_fScreenHeight;
    float m_fSidebarWidth;
    float m_fGridStartX;
    float m_fGridStartY;
    float m_fCardWidth;
    float m_fCardHeight;
    int m_nGridCols;
    int m_nGridRows;
    BOOL m_bLayoutValid;
};

#endif // STORESCENE_H
