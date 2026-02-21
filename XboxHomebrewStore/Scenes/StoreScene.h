#pragma once

#include "Scene.h"

#include "..\Main.h"
#include "..\Font.h"
#include "..\ImageDownloader.h"
#include "..\StoreManager.h"

class StoreScene : public Scene
{
public:
    explicit StoreScene();
    virtual ~StoreScene();
    virtual void Render();
    virtual void Update();

private:
    void RenderHeader();
    void RenderFooter();
    void RenderCategorySidebar();
    void RenderMainGrid();
    void DrawStoreItem(StoreItem* storeItem, int x, int y, bool selected, int slotIndex);

    ImageDownloader* mImageDownloader;
    bool mSideBarFocused;
    uint32_t mHighlightedCategoryIndex;
    uint32_t mStoreIndex;
};
