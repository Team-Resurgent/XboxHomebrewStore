#include "StoreScene.h"
#include "SceneManager.h"
#include "VersionScene.h"

#include "..\Main.h"
#include "..\Math.h"
#include "..\Defines.h"
#include "..\Context.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\String.h"
#include "..\InputManager.h"
#include "..\TextureHelper.h"
#include "..\StoreManager.h"
#include "..\Debug.h"

StoreScene::StoreScene()
{
    mImageDownloader = new ImageDownloader();
    mSideBarFocused = true;
    mHighlightedCategoryIndex = 0;
    mStoreIndex = 0;
}

StoreScene::~StoreScene()
{
}

void StoreScene::OnResume()
{
    StoreManager::SetCategoryIndex(StoreManager::GetCategoryIndex());
    mStoreIndex = 0;
}

void StoreScene::RenderHeader()
{
    Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0, Context::GetScreenWidth(), ASSET_HEADER_HEIGHT);
    Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
    Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386, 16, 12, ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
}

void StoreScene::RenderFooter()
{
    float footerY = Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0, footerY, Context::GetScreenWidth(), ASSET_FOOTER_HEIGHT);

    Drawing::DrawTexturedRect(TextureHelper::GetControllerIcon("StickLeft"), 0xffffffff, 16.0f, footerY + 10.0f, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
    Font::DrawText(FONT_NORMAL, "Hide A: Details B: Exit LT/RT: Category D-pad: Move", COLOR_WHITE, 52, footerY + 12);
}

void StoreScene::RenderCategorySidebar()
{
    float sidebarHeight = (Context::GetScreenHeight() - ASSET_SIDEBAR_Y) - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0, ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH, sidebarHeight);

    Font::DrawText(FONT_NORMAL, "Categories...", COLOR_WHITE, 16, ASSET_SIDEBAR_Y + 16);

    int32_t categoryCount = StoreManager::GetCategoryCount();

    int32_t maxItems = (int32_t)(sidebarHeight - 64) / 44;
    int32_t start = 0;
    if (categoryCount >= maxItems) {
        start = Math::ClampInt32(mHighlightedCategoryIndex - (maxItems / 2), 0, categoryCount - maxItems);
    }

    int32_t itemCount = Math::MinInt32(start + maxItems, categoryCount) - start;

    for (int32_t pass = 0; pass < 2; pass++) 
    {
        float y = ASSET_SIDEBAR_Y + 64;

        if (pass == 1) {
            Drawing::BeginStencil(48.0f, (float)ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH - 64.0f, (float)sidebarHeight);
        }

        for (int32_t i = 0; i < itemCount; i++)
        {
            int32_t index = start + i;

            StoreCategory* storeCategory = StoreManager::GetStoreCategory(index);

            bool highlighted = index == mHighlightedCategoryIndex;
            bool focused = mSideBarFocused && highlighted;

            if (pass == 0)
            {
                if (highlighted || focused)
                {
                    Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(), focused ? COLOR_FOCUS_HIGHLIGHT : COLOR_HIGHLIGHT, 0, y - 32, ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
                }

                bool activated = index == StoreManager::GetCategoryIndex();
                if (mSideBarFocused == true)
                {
                    Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(storeCategory->name), activated ? COLOR_FOCUS_HIGHLIGHT : 0xffffffff, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
                }
                else
                {
                    Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(storeCategory->name), activated ? COLOR_HIGHLIGHT : 0xffffffff, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
                }
            }
            else
            {
                if (focused == true) {
                    Font::DrawTextScrolling(FONT_NORMAL, storeCategory->name, COLOR_WHITE, 48, y, ASSET_SIDEBAR_WIDTH - 64, storeCategory->nameScrollState);
                } else {
                    storeCategory->nameScrollState.active = false;
                    Font::DrawText(FONT_NORMAL, storeCategory->name, COLOR_WHITE, 48, y);
                }
            }

            y += 44;
        }

        if (pass == 1) {
            Drawing::EndStencil();
        }
    }
}

void StoreScene::DrawStoreItem(StoreItem* storeItem, float x, float y, bool selected, int32_t slotIndex)
{
    Drawing::DrawTexturedRect(TextureHelper::GetCard(), 0xFFFFFFFF, x, y, ASSET_CARD_WIDTH, ASSET_CARD_HEIGHT);

    if (selected == true) {
        Drawing::DrawTexturedRect(TextureHelper::GetCardHighlight(), mSideBarFocused ? COLOR_HIGHLIGHT : COLOR_FOCUS_HIGHLIGHT, x - 3, y - 3, ASSET_CARD_HIGHLIGHT_WIDTH, ASSET_CARD_HIGHLIGHT_HEIGHT);
    }

    //144/204

    float iconW = ASSET_CARD_WIDTH - 18;
    float iconH = ASSET_CARD_HEIGHT - 62;
    float iconX = x + 9;
    float iconY = y + 9;

    D3DTexture* cover = storeItem->cover;
    if (cover == nullptr) 
    {
        if (ImageDownloader::IsCoverCached(storeItem->appId) == true)
        {
            storeItem->cover = TextureHelper::LoadFromFile(ImageDownloader::GetCoverCachePath(storeItem->appId));
            cover = storeItem->cover;
        }
        else
        {
            cover = TextureHelper::GetCover();
            mImageDownloader->Queue(&storeItem->cover, storeItem->appId, IMAGE_COVER);
        }
    }
    Drawing::DrawTexturedRect(cover, 0xFFFFFFFF, iconX, iconY, iconW, iconH);

    float textX   = x + 8;
    float nameY   = y + iconH + 14;
    float authorY = y + iconH + 32;

    Drawing::BeginStencil((float)x + 8, (float)(y + iconH), (float)ASSET_CARD_WIDTH - 16, (float)62);

    if (selected && !mSideBarFocused)
    {
        float textMaxWidth = ASSET_CARD_WIDTH - 16;
        Font::DrawTextScrolling(FONT_NORMAL, storeItem->name, COLOR_WHITE, textX, nameY, textMaxWidth, storeItem->nameScrollState);
        Font::DrawTextScrolling(FONT_NORMAL, storeItem->author, COLOR_TEXT_GRAY, textX, authorY, textMaxWidth, storeItem->authorScrollState);
    }
    else
    {
        storeItem->nameScrollState.active = false;
        storeItem->authorScrollState.active = false;
        Font::DrawText(FONT_NORMAL, storeItem->name, COLOR_WHITE, textX, nameY);
        Font::DrawText(FONT_NORMAL, storeItem->author, COLOR_TEXT_GRAY, textX, authorY);
    }

    Drawing::EndStencil();

    if (storeItem->state == 1)
    {
        Drawing::DrawTexturedRect(TextureHelper::GetNewBadge(), 0xFFFFFFFF, x + ASSET_CARD_WIDTH - ASSET_BADGE_NEW_WIDTH + 4, y - 12, ASSET_BADGE_NEW_WIDTH, ASSET_BADGE_NEW_HEIGHT);
    }
    else if (storeItem->state == 2)
    {
        Drawing::DrawTexturedRect(TextureHelper::GetUpdateBadge(), 0xFFFFFFFF, x + ASSET_CARD_WIDTH - ASSET_BADGE_UPDATE_WIDTH + 4, y - 12, ASSET_BADGE_UPDATE_WIDTH, ASSET_BADGE_UPDATE_HEIGHT);
    }
}

void StoreScene::RenderMainGrid()
{
    float gridX = ASSET_SIDEBAR_WIDTH;
    float gridY = ASSET_HEADER_HEIGHT;
    float gridWidth = Context::GetScreenWidth() - ASSET_SIDEBAR_WIDTH; 
    float gridHeight = Context::GetScreenHeight() - (ASSET_HEADER_HEIGHT + ASSET_FOOTER_HEIGHT);

    int32_t count = StoreManager::GetWindowStoreItemCount();
    if( count == 0 )
    {
        Font::DrawText(FONT_NORMAL, "No apps in this category.", (uint32_t)COLOR_TEXT_GRAY, gridX, gridY);
        return;
    }
  
    float cardWidth = ASSET_CARD_WIDTH;
    float cardHeight = ASSET_CARD_HEIGHT;
    float storeItemsWidth = (Context::GetGridCols() * (cardWidth + CARD_GAP)) - CARD_GAP;
    float storeItemsHeight = (Context::GetGridRows() * (cardHeight + CARD_GAP)) - CARD_GAP;

    float cardX = gridX + ((gridWidth - storeItemsWidth) / 2);
    float cardY = gridY + ((gridHeight - storeItemsHeight) / 2);


    int32_t totalSlots = Context::GetGridCells();
    int32_t windowCount = StoreManager::GetWindowStoreItemCount();
    int32_t slotsInView = Math::MinInt32(totalSlots, windowCount);
    for (int32_t currentSlot = 0; currentSlot < slotsInView; currentSlot++ )
    {
        int32_t row = currentSlot / Context::GetGridCols();
        int32_t col = currentSlot % Context::GetGridCols();
        float x = cardX + col * ( cardWidth + CARD_GAP);
        float y = cardY + row * ( cardHeight + CARD_GAP);
        StoreItem* storeItem = StoreManager::GetWindowStoreItem(currentSlot);
        DrawStoreItem(storeItem, x, y, currentSlot == (mStoreIndex - StoreManager::GetWindowStoreItemOffset()), currentSlot);
    }

    std::string pageStr = String::Format("Item %d of %d", mStoreIndex + 1, StoreManager::GetSelectedCategoryTotal());
    Font::DrawText(FONT_NORMAL, pageStr.c_str(), (uint32_t)COLOR_TEXT_GRAY, 600, Context::GetScreenHeight() - 30);
}

void StoreScene::Render()
{
    Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0, Context::GetScreenWidth(), Context::GetScreenHeight());
    
    RenderHeader();
    RenderFooter();
    RenderCategorySidebar();
    RenderMainGrid();
}

void StoreScene::Update()
{
    if (mSideBarFocused)
    {
        if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            mHighlightedCategoryIndex = mHighlightedCategoryIndex > 0 ? mHighlightedCategoryIndex - 1 : StoreManager::GetCategoryCount() - 1;
        }
        else if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            mHighlightedCategoryIndex = mHighlightedCategoryIndex < StoreManager::GetCategoryCount() - 1 ? mHighlightedCategoryIndex + 1 : 0;
        }
        else if (InputManager::ControllerPressed(ControllerA, -1))
        {
            bool needsUpdate = StoreManager::GetCategoryIndex() != mHighlightedCategoryIndex;
            if (needsUpdate == true) {
                StoreManager::SetCategoryIndex(mHighlightedCategoryIndex);
                mStoreIndex = 0;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadRight, -1))
        {
            mSideBarFocused = false;
        }
    }
    else
    {
        int32_t col = mStoreIndex % Context::GetGridCols();
        int32_t row = mStoreIndex / Context::GetGridCols();
        if (InputManager::ControllerPressed(ControllerDpadLeft, -1))
        {
            if (col == 0) {
                mSideBarFocused = true;
            } else {
                mStoreIndex--;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadRight, -1))
        {
            if (col < (Context::GetGridCols() - 1) && mStoreIndex < (StoreManager::GetSelectedCategoryTotal() - 1)) {
                mStoreIndex++;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            if ((mStoreIndex - StoreManager::GetWindowStoreItemOffset()) >= Context::GetGridCols())
            {
                mStoreIndex -= Context::GetGridCols();
            }
            else if (StoreManager::HasPrevious())
            {
                StoreManager::LoadPrevious();
                mStoreIndex = mStoreIndex >= Context::GetGridCols() ? mStoreIndex - Context::GetGridCols() : 0;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            if (mStoreIndex < (StoreManager::GetWindowStoreItemOffset() + StoreManager::GetWindowStoreItemCount() - Context::GetGridCols()))
            {
                mStoreIndex += Context::GetGridCols();
            }
            else if (StoreManager::HasNext())
            {
                mImageDownloader->CancelAll();
                StoreManager::LoadNext();
                mStoreIndex = Math::MinInt32(mStoreIndex + Context::GetGridCols(), StoreManager::GetSelectedCategoryTotal() - 1);
            }
        }
        else if (InputManager::ControllerPressed(ControllerA, -1))
        {
            StoreVersions storeVersions;
            if (StoreManager::TryGetStoreVersions(mStoreIndex - StoreManager::GetWindowStoreItemOffset(), &storeVersions))
            {
                SceneManager* sceneManager = Context::GetSceneManager();
                sceneManager->PushScene(new VersionScene(storeVersions));
            }
        }
    }
}
