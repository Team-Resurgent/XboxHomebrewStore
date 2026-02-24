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
    virtual void OnResume();

private:
    void RenderHeader();
    void RenderFooter();
    void RenderCategorySidebar();
    void RenderMainGrid();
    void DrawStoreItem(StoreItem* storeItem, float x, float y, bool selected, int32_t slotIndex);

    ImageDownloader* mImageDownloader;
    bool mSideBarFocused;
    int32_t mHighlightedCategoryIndex;
    int32_t mStoreIndex;
};
