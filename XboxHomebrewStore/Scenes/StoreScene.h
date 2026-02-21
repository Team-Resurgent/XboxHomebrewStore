#pragma once

#include "Scene.h"

#include "..\Main.h"
#include "..\Store.h"
#include "..\Font.h"
#include "..\ImageDownloader.h"

class StoreScene : public Scene
{
public:
    explicit StoreScene();
    virtual ~StoreScene();
    virtual void Render();
    virtual void Update();

private:
    void CalculateLayout();
    void HandleInput();
    void RenderHeader();
    void RenderFooter();
    void RenderCategorySidebar();
    void RenderMainGrid();
    void DrawStoreItem(StoreItem* storeItem, int x, int y, bool selected, int slotIndex);
    void RenderDownloading();
    void RenderSettings();

    UIState m_CurrentState;
    BOOL m_bFocusOnSidebar;
    int m_nSelectedCol;
    int m_nSelectedRow;
    int m_nSelectedItem;
    int m_nScrollOffset;
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

    ImageDownloader* mImageDownloader;
    bool mSideBarFocused;
    uint32_t mHighlightedCategoryIndex;
    uint32_t mStoreIndex;
};
