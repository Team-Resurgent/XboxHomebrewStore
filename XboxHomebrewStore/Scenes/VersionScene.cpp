#include "VersionScene.h"
#include "SceneManager.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\Math.h"
#include "..\String.h"
#include "..\InputManager.h"
#include "..\TextureHelper.h"
#include "..\WebManager.h"
#include "..\UserState.h"
#include "..\FileSystem.h"

VersionScene::VersionScene(const StoreVersions& storeVersions)
{
    mImageDownloader = new ImageDownloader();

    mStoreVersions = storeVersions;
    mSideBarFocused = true;
    mHighlightedVersionIndex = 0;
    mVersionIndex = 0;
    mListViewScrollOffset = 0.0f;
    mListViewContentHeight = 0.0f;
    mDescriptionHeight = 0.0f;
    mChangeLogHeight = 0.0f;
    mLastMeasuredVersionIndex = -1;

    mDownloading = false;
    mDownloadCancelRequested = false;
    mDownloadNow = 0;
    mDownloadTotal = 0;
    mDownloadSuccess = false;
    mDownloadThread = nullptr;
    mUnpacking = false;
    mUnpackCancelRequested = false;
    mUnpackCurrent = 0;
    mUnpackTotal = 0;

    const float infoXPos = 350.0f;
    float descMaxWidth = (float)Context::GetScreenWidth() - infoXPos - 20.0f;
    Font::MeasureTextWrapped(FONT_NORMAL, mStoreVersions.description, descMaxWidth, nullptr, &mDescriptionHeight);
    if (!mStoreVersions.versions.empty()) {
        Font::MeasureTextWrapped(FONT_NORMAL, mStoreVersions.versions[0].changeLog, descMaxWidth, nullptr, &mChangeLogHeight);
        mLastMeasuredVersionIndex = 0;
    }
}

VersionScene::~VersionScene()
{
    if (mStoreVersions.cover != nullptr)
    {
        mStoreVersions.cover->Release();
    }
    if (mStoreVersions.screenshot != nullptr)
    {
        mStoreVersions.screenshot->Release();
    }
    if (mDownloadThread != nullptr) {
        mDownloadCancelRequested = true;
        WaitForSingleObject(mDownloadThread, INFINITE);
        CloseHandle(mDownloadThread);
        mDownloadThread = nullptr;
    }
}

void VersionScene::Render()
{
    Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0.0f, 0, Context::GetScreenWidth(), Context::GetScreenHeight());
    
    RenderHeader();
    RenderFooter();
    RenderVersionSidebar();
    RenderListView();

    if (mDownloading) {
        RenderDownloadOverlay();
    }
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
    const char* footerText = mUnpacking
        ? "Unpacking... B: Cancel"
        : (mDownloading ? "Downloading... B: Cancel" : "A: Download B: Exit D-pad: Move");
    Font::DrawText(FONT_NORMAL, footerText, COLOR_WHITE, 52, footerY + 12);
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
                    Font::DrawTextScrolling(FONT_NORMAL, storeVersion->version, COLOR_WHITE, 16.0f, y, ASSET_SIDEBAR_WIDTH - 64.0f, storeVersion->versionScrollState);
                } else {
                    storeVersion->versionScrollState.active = false;
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
    const float titleXPos = 200.0f;
    const float infoXPos = 350.0f;
    const float infoMaxWidth = (float)Context::GetScreenWidth() - infoXPos - 20.0f;

    StoreVersion* storeVersion = &mStoreVersions.versions[mHighlightedVersionIndex];

    if (mHighlightedVersionIndex != mLastMeasuredVersionIndex) {
        Font::MeasureTextWrapped(FONT_NORMAL, storeVersion->changeLog, infoMaxWidth, nullptr, &mChangeLogHeight);
        mLastMeasuredVersionIndex = mHighlightedVersionIndex;
    }

    mListViewContentHeight = (float)ASSET_SCREENSHOT_HEIGHT + 16.0f + 30.0f + 30.0f + mDescriptionHeight + 8.0f
        + 30.0f * 5.0f + mChangeLogHeight + 8.0f + 30.0f;
    float listVisibleHeight = (float)(Context::GetScreenHeight() - ASSET_SIDEBAR_Y - ASSET_FOOTER_HEIGHT);
    float maxScroll = mListViewContentHeight - listVisibleHeight;
    if (maxScroll < 0.0f) {
        maxScroll = 0.0f;
    }
    if (mListViewScrollOffset > maxScroll) {
        mListViewScrollOffset = maxScroll;
    }
    if (mListViewScrollOffset < 0.0f) {
        mListViewScrollOffset = 0.0f;
    }

    float gridX = ASSET_SIDEBAR_WIDTH;
    float gridY = (float)ASSET_SIDEBAR_Y - mListViewScrollOffset;

    Drawing::BeginStencil((float)ASSET_SIDEBAR_WIDTH, (float)ASSET_SIDEBAR_Y, (float)(Context::GetScreenWidth() - ASSET_SIDEBAR_WIDTH), listVisibleHeight);

    D3DTexture* screenshot = mStoreVersions.screenshot;
    if (screenshot == nullptr) 
    {
        if (ImageDownloader::IsScreenshotCached(mStoreVersions.appId) == true)
        {
            mStoreVersions.screenshot = TextureHelper::LoadFromFile(ImageDownloader::GetScreenshotCachePath(mStoreVersions.appId));
            screenshot = mStoreVersions.screenshot;
        }
        else
        {
            screenshot = TextureHelper::GetScreenshot();
            mImageDownloader->Queue(&mStoreVersions.screenshot, mStoreVersions.appId, IMAGE_SCREENSHOT);
        }
    }
    Drawing::DrawTexturedRect(screenshot, 0xFFFFFFFF, titleXPos, gridY, ASSET_SCREENSHOT_WIDTH, ASSET_SCREENSHOT_HEIGHT);

    D3DTexture* cover = mStoreVersions.cover;
    if (cover == nullptr) 
    {
        if (ImageDownloader::IsCoverCached(mStoreVersions.appId) == true)
        {
            mStoreVersions.cover = TextureHelper::LoadFromFile(ImageDownloader::GetCoverCachePath(mStoreVersions.appId));
            cover = mStoreVersions.cover;
        }
        else
        {
            cover = TextureHelper::GetCover();
            mImageDownloader->Queue(&mStoreVersions.cover, mStoreVersions.appId, IMAGE_COVER);
        }
    }
    Drawing::DrawTexturedRect(cover, 0xFFFFFFFF, 216 + ASSET_SCREENSHOT_WIDTH, gridY, 144, 204);

    gridY += ASSET_SCREENSHOT_HEIGHT + 16;

    Font::DrawText(FONT_NORMAL, "Name:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.name, COLOR_TEXT_GRAY, infoXPos, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Author:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawText(FONT_NORMAL, mStoreVersions.author, COLOR_TEXT_GRAY, infoXPos, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Description:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawTextWrapped(FONT_NORMAL, mStoreVersions.description, COLOR_TEXT_GRAY, infoXPos, gridY, infoMaxWidth);
    gridY += mDescriptionHeight + 8.0f;

    Font::DrawText(FONT_NORMAL, "Version:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->version, COLOR_TEXT_GRAY, infoXPos, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Title ID:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->titleId, COLOR_TEXT_GRAY, infoXPos, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Release Date:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->releaseDate, COLOR_TEXT_GRAY, infoXPos, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Region:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawText(FONT_NORMAL, storeVersion->region, COLOR_TEXT_GRAY, infoXPos, gridY);
    gridY += 30;
    Font::DrawText(FONT_NORMAL, "Change Log:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawTextWrapped(FONT_NORMAL, storeVersion->changeLog, COLOR_TEXT_GRAY, infoXPos, gridY, infoMaxWidth);
    gridY += mChangeLogHeight + 8.0f;
    Font::DrawText(FONT_NORMAL, "Size:", COLOR_WHITE, titleXPos, gridY);
    Font::DrawText(FONT_NORMAL, String::FormatSize(storeVersion->size), COLOR_TEXT_GRAY, infoXPos, gridY);

    Drawing::EndStencil();
}

void VersionScene::RenderDownloadOverlay()
{
    const float w = Context::GetScreenWidth();
    const float h = Context::GetScreenHeight();

    Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, (float)w, (float)h);

    const float panelWidth = 400.0f;
    const float panelHeight = 140.0f;
    float panelX = ((float)w - panelWidth) * 0.5f;
    float panelY = ((float)h - panelHeight) * 0.5f;

    Drawing::DrawFilledRect(COLOR_CARD_BG, panelX, panelY, panelWidth, panelHeight);

    const float barY = panelY + 50.0f;
    const float barH = 24.0f;
    const float barMargin = 20.0f;
    float barW = panelWidth - barMargin * 2.0f;
    float barX = panelX + barMargin;

    if (mUnpacking) {
        Font::DrawText(FONT_NORMAL, "Unpacking...", COLOR_WHITE, panelX + 20.0f, panelY + 16.0f);
        Drawing::DrawFilledRect(COLOR_SECONDARY, barX, barY, barW, barH);
        int total = mUnpackTotal;
        if (total > 0) {
            float pct = (float)mUnpackCurrent / (float)total;
            if (pct > 1.0f) pct = 1.0f;
            Drawing::DrawFilledRect(COLOR_DOWNLOAD, barX, barY, barW * pct, barH);
        }
        std::string progressStr = total > 0
            ? String::Format("%d / %d files", mUnpackCurrent, total)
            : "Preparing...";
        Font::DrawText(FONT_NORMAL, progressStr, COLOR_TEXT_GRAY, panelX + 20.0f, barY + barH + 8.0f);
        Font::DrawText(FONT_NORMAL, "B: Cancel", COLOR_WHITE, panelX + 20.0f, panelY + panelHeight - 28.0f);
    } else {
        Font::DrawText(FONT_NORMAL, "Downloading...", COLOR_WHITE, panelX + 20.0f, panelY + 16.0f);
        Drawing::DrawFilledRect(COLOR_SECONDARY, barX, barY, barW, barH);
        uint32_t total = mDownloadTotal;
        if (total > 0) {
            float pct = (float)mDownloadNow / (float)total;
            if (pct > 1.0f) pct = 1.0f;
            Drawing::DrawFilledRect(COLOR_DOWNLOAD, barX, barY, barW * pct, barH);
        }
        std::string progressStr = total > 0
            ? String::Format("%s / %s", String::FormatSize(mDownloadNow).c_str(), String::FormatSize(total).c_str())
            : "Connecting...";
        Font::DrawText(FONT_NORMAL, progressStr, COLOR_TEXT_GRAY, panelX + 20.0f, barY + barH + 8.0f);
        Font::DrawText(FONT_NORMAL, "B: Cancel", COLOR_WHITE, panelX + 20.0f, panelY + panelHeight - 28.0f);
    }
}

void VersionScene::DownloadProgressCb(uint32_t dlNow, uint32_t dlTotal, void* userData)
{
    VersionScene* scene = (VersionScene*)userData;
    if (scene != nullptr) {
        scene->mDownloadNow = dlNow;
        scene->mDownloadTotal = (uint32_t)dlTotal;
    }
}

bool VersionScene::UnpackProgressCb(int currentFile, int totalFiles, const char* currentFileName, void* userData)
{
    (void)currentFileName;
    VersionScene* scene = (VersionScene*)userData;
    if (scene != nullptr) {
        if (scene->mUnpackCancelRequested) {
            return false;
        }
        scene->mUnpackCurrent = currentFile;
        scene->mUnpackTotal = totalFiles;
    }
    return true;
}

static bool EndsWithZip(const std::string path)
{
    std::string ext = FileSystem::GetExtension(path);
    if (ext.size() != 4 || ext[0] != '.') return false;
    return (ext[1] == 'z' || ext[1] == 'Z') && (ext[2] == 'i' || ext[2] == 'I') && (ext[3] == 'p' || ext[3] == 'P');
}

DWORD WINAPI VersionScene::DownloadThreadProc(LPVOID param)
{
    VersionScene* scene = (VersionScene*)param;
    if (scene == nullptr) return 0;

    StoreVersion* ver = &scene->mStoreVersions.versions[scene->mHighlightedVersionIndex];
    std::string versionId = ver->versionId;
    const std::string baseDir = "HDD0-E:\\Homebrew\\Downloads\\";

    bool ok = !ver->downloadFiles.empty();
    for (size_t f = 0; ok && f < ver->downloadFiles.size(); f++)
    {
        if (scene->mDownloadCancelRequested) {
            ok = false;
            break;
        }
        const std::string& entry = ver->downloadFiles[f];
        bool isUrl = (entry.size() >= 8 && entry.compare(0, 8, "https://") == 0)
            || (entry.size() >= 7 && entry.compare(0, 7, "http://") == 0);
        std::string filePath = FileSystem::CombinePath(baseDir, isUrl ? FileSystem::GetFileName(entry) : entry);
        if (isUrl) {
            ok = WebManager::TryDownload(entry, filePath, DownloadProgressCb, scene, (volatile bool*)&scene->mDownloadCancelRequested);
        } else {
            ok = WebManager::TryDownloadVersionFile(
                versionId,
                (int32_t)f,
                filePath,
                DownloadProgressCb,
                scene,
                (volatile bool*)&scene->mDownloadCancelRequested
            );
        }
    }

    if (ok) {
        std::string installPath = FileSystem::CombinePath(baseDir, ver->folderName);
        FileSystem::DirectoryCreate(installPath);

        scene->mUnpacking = true;
        scene->mUnpackCancelRequested = false;
        scene->mUnpackCurrent = 0;
        scene->mUnpackTotal = 0;
        bool unpackOk = true;
        for (size_t f = 0; unpackOk && f < ver->downloadFiles.size(); f++)
        {
            const std::string& entry = ver->downloadFiles[f];
            if (!EndsWithZip(entry)) continue;
            bool isUrl = (entry.size() >= 8 && entry.compare(0, 8, "https://") == 0)
                || (entry.size() >= 7 && entry.compare(0, 7, "http://") == 0);
            std::string zipPath = FileSystem::CombinePath(baseDir, isUrl ? FileSystem::GetFileName(entry) : entry);
            unpackOk = xunzipFromFile(zipPath.c_str(), installPath.c_str(), true, true, false, UnpackProgressCb, scene);
        }
        scene->mUnpacking = false;

        if (unpackOk) {
            UserSaveState userSaveState;
            memset(&userSaveState, 0, sizeof(UserSaveState));
            strcpy(userSaveState.appId, ver->appId.c_str());
            strcpy(userSaveState.versionId, ver->versionId.c_str());
            strcpy(userSaveState.downloadPath, baseDir.c_str());
            strcpy(userSaveState.installPath, installPath.c_str());
            UserState::TrySave(&userSaveState);
        }
    }

    scene->mDownloadSuccess = ok;
    scene->mDownloading = false;
    return 0;
}

void VersionScene::StartDownload()
{
    if (mDownloading || mStoreVersions.versions.empty()) return;

    if (mDownloadThread != nullptr) {
        WaitForSingleObject(mDownloadThread, INFINITE);
        CloseHandle(mDownloadThread);
        mDownloadThread = nullptr;
    }

    mDownloadCancelRequested = false;
    mDownloadNow = 0;
    mDownloadTotal = 0;
    mDownloading = true;

    mDownloadThread = CreateThread(nullptr, 0, DownloadThreadProc, this, 0, nullptr);
    if (mDownloadThread == nullptr) {
        mDownloading = false;
    }
}

void VersionScene::Update()
{
    if (mDownloading) {
        if (InputManager::ControllerPressed(ControllerB, -1)) {
            if (mUnpacking) {
                mUnpackCancelRequested = true;
            } else {
                mDownloadCancelRequested = true;
            }
        }
        return;
    }

    const float listScrollStep = 44.0f;
    float listVisibleHeight = (float)(Context::GetScreenHeight() - ASSET_SIDEBAR_Y - ASSET_FOOTER_HEIGHT);
    float listMaxScroll = mListViewContentHeight - listVisibleHeight;
    if (listMaxScroll < 0.0f) {
        listMaxScroll = 0.0f;
    }

    if (mSideBarFocused)
    {
        if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            mHighlightedVersionIndex = mHighlightedVersionIndex > 0 ? mHighlightedVersionIndex - 1 : (int32_t)mStoreVersions.versions.size() - 1;
        }
        else if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            mHighlightedVersionIndex = mHighlightedVersionIndex < (int32_t)mStoreVersions.versions.size() - 1 ? mHighlightedVersionIndex + 1 : 0;
        }
        else if (InputManager::ControllerPressed(ControllerA, -1))
        {
            StartDownload();
        }
        else if (InputManager::ControllerPressed(ControllerB, -1))
        {
            SceneManager* sceneManager = Context::GetSceneManager();
            sceneManager->PopScene();
        }

        ControllerState state;
        if (InputManager::TryGetControllerState(-1, &state) && listMaxScroll > 0.0f)
        {
            mListViewScrollOffset -= state.ThumbRightDy;
            if (mListViewScrollOffset < 0.0f) mListViewScrollOffset = 0.0f;
            if (mListViewScrollOffset > listMaxScroll) mListViewScrollOffset = listMaxScroll;
        }
    }
}
