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

void StoreScene::RenderHeader()
{
    Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0, Context::GetScreenWidth(), ASSET_HEADER_HEIGHT);
    Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
    Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386, 16, 12, ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
}

void StoreScene::RenderFooter()
{
    int32_t footerY = Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0, footerY, Context::GetScreenWidth(), ASSET_FOOTER_HEIGHT);

    Drawing::DrawTexturedRect(TextureHelper::GetControllerIcon("StickLeft"), 0xffffffff, 16, footerY + 10, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
    Font::DrawText(FONT_NORMAL, "Hide A: Details B: Exit LT/RT: Category D-pad: Move", COLOR_WHITE, 52, footerY + 12);
}

void StoreScene::RenderCategorySidebar()
{
    int32_t sidebarHeight = (Context::GetScreenHeight() - ASSET_SIDEBAR_Y) - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0, ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH, sidebarHeight);

    int32_t y = ASSET_SIDEBAR_Y + 30;
    uint32_t categoryCount = StoreManager::GetCategoryCount();

    Drawing::BeginStencil(48.0f, (float)ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH - 64.0f, (float)sidebarHeight);
    for (uint32_t i = 0; i < categoryCount; i++)
    {
        StoreCategory* storeCategory = StoreManager::GetStoreCategory(i);

        bool highlighted = i == mHighlightedCategoryIndex;
        bool focused = mSideBarFocused && highlighted;

        if (focused == true) {
            Font::DrawTextScrolling(FONT_NORMAL, storeCategory->name, COLOR_WHITE, 48, y, ASSET_SIDEBAR_WIDTH - 64, &storeCategory->nameScrollState);
        } else {
            Font::DrawText(FONT_NORMAL, storeCategory->name, COLOR_WHITE, 48, y);
        }

        y += 44;
    }
    Drawing::EndStencil();

    y = ASSET_SIDEBAR_Y + 30;

    for (uint32_t i = 0; i < categoryCount; i++)
    {
        StoreCategory* storeCategory = StoreManager::GetStoreCategory(i);

        bool highlighted = i == mHighlightedCategoryIndex;
        bool focused = mSideBarFocused && highlighted;
        if (highlighted || focused)
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(), focused ? COLOR_FOCUS_HIGHLIGHT : COLOR_HIGHLIGHT, 0, y - 32, ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
        }

        bool activated = i == StoreManager::GetCategoryIndex();
        if (mSideBarFocused == true)
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(storeCategory->name), activated ? COLOR_FOCUS_HIGHLIGHT : 0xffffffff, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }
        else
        {
            Drawing::DrawTexturedRect(TextureHelper::GetCategoryIcon(storeCategory->name), activated ? COLOR_HIGHLIGHT : 0xffffffff, 16, y - 2, ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }

        y += 44;
    }
}

void StoreScene::DrawStoreItem(StoreItem* storeItem, int x, int y, bool selected, int slotIndex)
{
    Drawing::DrawTexturedRect(TextureHelper::GetCard(), 0xFFFFFFFF, x, y, ASSET_CARD_WIDTH, ASSET_CARD_HEIGHT);

    if (selected == true) {
        Drawing::DrawTexturedRect(TextureHelper::GetCardHighlight(), mSideBarFocused ? COLOR_HIGHLIGHT : COLOR_FOCUS_HIGHLIGHT, x - 3, y - 3, ASSET_CARD_HIGHLIGHT_WIDTH, ASSET_CARD_HIGHLIGHT_HEIGHT);
    }

    //144/204

    int iconW = ASSET_CARD_WIDTH - 18;
    int iconH = ASSET_CARD_HEIGHT - 62;
    int iconX = x + 9;
    int iconY = y + 9;

    Drawing::DrawFilledRect(COLOR_SECONDARY, iconX, iconY, iconW, iconH);
    D3DTexture* cover = storeItem->cover;
    if (cover == NULL) 
    {
        if (ImageDownloader::IsCoverCached(storeItem->id) == true)
        {
            storeItem->cover = TextureHelper::LoadFromFile(ImageDownloader::GetCoverCachePath(storeItem->id));
            cover = storeItem->cover;
        }
        else
        {
            cover = TextureHelper::GetCoverRef();
            mImageDownloader->Queue(&storeItem->cover, storeItem->id, IMAGE_COVER);
        }
    }
    Drawing::DrawTexturedRect(cover, 0xFFFFFFFF, iconX, iconY, iconW, iconH);

    int textX   = x + 8;
    int nameY   = y + iconH + 14;
    int authorY = y + iconH + 32;

    Drawing::BeginStencil((float)x + 8, (float)(y + iconH), (float)ASSET_CARD_WIDTH - 16, (float)62);

    if (selected && !mSideBarFocused)
    {
        int textMaxWidth = ASSET_CARD_WIDTH - 16;
        Font::DrawTextScrolling(FONT_NORMAL, storeItem->name, COLOR_WHITE, textX, nameY, textMaxWidth, &storeItem->nameScrollState);
        Font::DrawTextScrolling(FONT_NORMAL, storeItem->author, COLOR_TEXT_GRAY, textX, authorY, textMaxWidth, &storeItem->authorScrollState);
    }
    else
    {
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
        Drawing::DrawTexturedRect(TextureHelper::GetNewBadge(), 0xFFFFFFFF, x + ASSET_CARD_WIDTH - ASSET_BADGE_UPDATE_WIDTH + 4, y - 12, ASSET_BADGE_UPDATE_WIDTH, ASSET_BADGE_UPDATE_HEIGHT);
    }
}

void StoreScene::RenderMainGrid()
{
    int gridX = ASSET_SIDEBAR_WIDTH;
    int gridY = ASSET_HEADER_HEIGHT;
    int gridWidth = Context::GetScreenWidth() - ASSET_SIDEBAR_WIDTH; 
    int gridHeight = Context::GetScreenHeight() - (ASSET_HEADER_HEIGHT + ASSET_FOOTER_HEIGHT);

    int count = StoreManager::GetWindowStoreItemCount();
    if( count == 0 )
    {
        Font::DrawText(FONT_NORMAL, "No apps in this category.", (uint32_t)COLOR_TEXT_GRAY, gridX, gridY);
        return;
    }
  
    int cardWidth = ASSET_CARD_WIDTH;
    int cardHeight = ASSET_CARD_HEIGHT;
    int storeItemsWidth = (STORE_GRID_COLS * (cardWidth + CARD_GAP)) - CARD_GAP;
    int storeItemsHeight = (STORE_GRID_ROWS * (cardHeight + CARD_GAP)) - CARD_GAP;

    int cardX = gridX + ((gridWidth - storeItemsWidth) / 2);
    int cardY = gridY + ((gridHeight - storeItemsHeight) / 2);;


    uint32_t totalSlots = STORE_GRID_COLS * STORE_GRID_ROWS;
    uint32_t windowCount = StoreManager::GetWindowStoreItemCount();
    uint32_t slotsInView = Math::MinInt32(totalSlots, windowCount);
    for (uint32_t currentSlot = 0; currentSlot < slotsInView; currentSlot++ )
    {
        int row = currentSlot / STORE_GRID_COLS;
        int col = currentSlot % STORE_GRID_COLS;
        int x = cardX + col * ( cardWidth + CARD_GAP);
        int y = cardY + row * ( cardHeight + CARD_GAP);
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
        int col = mStoreIndex % STORE_GRID_COLS;
        int row = mStoreIndex / STORE_GRID_COLS;
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
            if (col < (STORE_GRID_COLS - 1) && mStoreIndex < (StoreManager::GetSelectedCategoryTotal() - 1)) {
                mStoreIndex++;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            if ((mStoreIndex - StoreManager::GetWindowStoreItemOffset()) >= STORE_GRID_COLS)
            {
                mStoreIndex -= STORE_GRID_COLS;
            }
            else if (StoreManager::HasPrevious())
            {
                StoreManager::LoadPrevious();
                mStoreIndex = mStoreIndex >= STORE_GRID_COLS ? mStoreIndex - STORE_GRID_COLS : 0;
            }
        }
        else if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            if (mStoreIndex < (StoreManager::GetWindowStoreItemOffset() + StoreManager::GetWindowStoreItemCount() - STORE_GRID_COLS))
            {
                mStoreIndex += STORE_GRID_COLS;
            }
            else if (StoreManager::HasNext())
            {
                mImageDownloader->CancelAll();
                StoreManager::LoadNext();
                mStoreIndex = Math::MinUint32(mStoreIndex + STORE_GRID_COLS, StoreManager::GetSelectedCategoryTotal() - 1);
            }
        }
    }
}
