#include "VersionScene.h"
#include "VersionDetailsScene.h"
#include "SceneManager.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\Math.h"
#include "..\String.h"
#include "..\InputManager.h"
#include "..\TextureHelper.h"

VersionScene::VersionScene(const StoreVersions& storeVersions)
{
    mStoreVersions = storeVersions;
    mSideBarFocused = true;
    mHighlightedVersionIndex = 0;
    mVersionIndex = 0;
}

void VersionScene::Render()
{
    Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0.0f, 0, Context::GetScreenWidth(), Context::GetScreenHeight());
    
    RenderHeader();
    RenderFooter();
    RenderVersionSidebar();
    RenderListView();
}

void VersionScene::RenderHeader()
{
    Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0, Context::GetScreenWidth(), ASSET_HEADER_HEIGHT);
    Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
    Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386, 16, 12, ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
}

void VersionScene::RenderFooter()
{
    float footerY = Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0.0f, footerY, Context::GetScreenWidth(), ASSET_FOOTER_HEIGHT);

    Drawing::DrawTexturedRect(TextureHelper::GetControllerIcon("StickLeft"), 0xffffffff, 16.0f, footerY + 10, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
    Font::DrawText(FONT_NORMAL, "Hide A: Details B: Exit LT/RT: Category D-pad: Move", COLOR_WHITE, 52, footerY + 12);
}

void VersionScene::RenderVersionSidebar()
{
    float sidebarHeight = (Context::GetScreenHeight() - ASSET_SIDEBAR_Y) - ASSET_FOOTER_HEIGHT;
    Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0, ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH, sidebarHeight);

    Font::DrawText(FONT_NORMAL, "Versions...", COLOR_WHITE, 16, ASSET_SIDEBAR_Y + 16);

    int32_t versionCount = mStoreVersions.versions.size();

    int32_t maxItems = (int32_t)(sidebarHeight - 64) / 44;
    int32_t start = 0;
    if (versionCount >= maxItems) {
        start = Math::ClampInt32(mHighlightedVersionIndex - (maxItems / 2), 0, versionCount - maxItems);
    }

    int32_t itemCount = Math::MinInt32(start + maxItems, versionCount) - start;

    for (int32_t pass = 0; pass < 2; pass++) 
    {
        float y = ASSET_SIDEBAR_Y + 64.0f;

        if (pass == 1) {
            Drawing::BeginStencil(16, (float)ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH - 32.0f, (float)sidebarHeight);
        }

        for (int32_t i = 0; i < itemCount; i++)
        {
            int32_t index = start + i;

            StoreVersion* storeVersion = &mStoreVersions.versions[index];

            bool highlighted = index == mHighlightedVersionIndex;
            bool focused = mSideBarFocused && highlighted;

            if (pass == 0)
            {
                if (highlighted || focused)
                {
                    Drawing::DrawTexturedRect(TextureHelper::GetCategoryHighlight(), focused ? COLOR_FOCUS_HIGHLIGHT : COLOR_HIGHLIGHT, 0.0f, y - 32.0f, ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
                }
            }
            else
            {
                if (focused == true) {
                    Font::DrawTextScrolling(FONT_NORMAL, storeVersion->version, COLOR_WHITE, 16.0f, y, ASSET_SIDEBAR_WIDTH - 64.0f, &storeVersion->versionScrollState);
                } else {
                    Font::DrawText(FONT_NORMAL, storeVersion->version, COLOR_WHITE, 16, y);
                }
            }

            y += 44;
        }

        if (pass == 1) {
            Drawing::EndStencil();
        }
    }
}

void VersionScene::RenderListView()
{
    float gridX = ASSET_SIDEBAR_WIDTH;
    float gridY = ASSET_SIDEBAR_Y;

    if (mStoreVersions.screenshot ) {
        Drawing::DrawTexturedRect(mStoreVersions.screenshot, 0xFFFFFFFF, (int)200, gridY, ASSET_SCREENSHOT_WIDTH, ASSET_SCREENSHOT_HEIGHT);
    } else {
        Drawing::DrawTexturedRect(TextureHelper::GetScreenshotRef(), 0xFFFFFFFF, (int)200, gridY, ASSET_SCREENSHOT_WIDTH, ASSET_SCREENSHOT_HEIGHT);
    }

    Drawing::DrawTexturedRect(TextureHelper::GetCoverRef(), 0xFFFFFFFF, 216 + ASSET_SCREENSHOT_WIDTH, gridY, 144, 204);

    gridY += ASSET_SCREENSHOT_HEIGHT + 16;

    Font::DrawText(FONT_NORMAL, "Name:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.name, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Author:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.author, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Description:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.description, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;

    StoreVersion* storeVersion = &mStoreVersions.versions[mHighlightedVersionIndex];

    Font::DrawText(FONT_NORMAL, "Version:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->version, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Title ID:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->titleId, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Release Date:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->releaseDate, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Region:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->region, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Change Log:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->changeLog, COLOR_TEXT_GRAY, 350.0f, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Size:", COLOR_WHITE, 200.0f, gridY);
    Font::DrawText(FONT_NORMAL, String::FormatSize(storeVersion->size), COLOR_TEXT_GRAY, 350.0f, gridY);
}

void VersionScene::Update()
{
    if (mSideBarFocused)
    {
        if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            mHighlightedVersionIndex = mHighlightedVersionIndex > 0 ? mHighlightedVersionIndex - 1 : StoreManager::GetCategoryCount() - 1;
        }
        else if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            mHighlightedVersionIndex = mHighlightedVersionIndex < StoreManager::GetCategoryCount() - 1 ? mHighlightedVersionIndex + 1 : 0;
        }
        else if (InputManager::ControllerPressed(ControllerA, -1))
        {
           /* bool needsUpdate = StoreManager::GetCategoryIndex() != mHighlightedCategoryIndex;
            if (needsUpdate == true) {
                StoreManager::SetCategoryIndex(mHighlightedCategoryIndex);
                mStoreIndex = 0;
            }*/
        }
        else if (InputManager::ControllerPressed(ControllerB, -1))
        {
            SceneManager* sceneManager = Context::GetSceneManager();
            sceneManager->PopScene();
        }
        else if (InputManager::ControllerPressed(ControllerDpadRight, -1))
        {
            //mSideBarFocused = false;
        }
    }
}
