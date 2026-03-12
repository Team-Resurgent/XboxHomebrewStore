#include "StoreScene.h"
#include "SceneManager.h"
#include "SettingsScene.h"
#include "VersionScene.h"

#include "..\AppSettings.h"
#include "..\TextureCache.h"
#include "..\Context.h"
#include "..\Debug.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\ImageDownloader.h"
#include "..\InputManager.h"
#include "..\Main.h"
#include "..\Math.h"
#include "..\Network.h"
#include "..\StoreList.h"
#include "..\StoreManager.h"
#include "..\MetaCache.h"
#include "..\String.h"
#include "..\TextureHelper.h"
#include "..\ViewState.h"

StoreScene::StoreScene() {
  mImageDownloader = new ImageDownloader();
  ImageDownloader::ResetCachedCoverCount(); // one-time scan before converter thread runs
  mSideBarFocused = true;
  mHighlightedCategoryIndex = 0;
  mStoreIndex = 0;
  mSavedStoreIndex = 0;
  mSavedWindowOffset = 0;
  mLastQueueOffset = -1;
  mHasSavedPosition = false;
  mShowInfoOverlay = false;
  mInfoOverlayMessage = "";
  mIdleFrames = 0;
  mMetaReady = false;
}

StoreScene::~StoreScene() { StoreManager::StopIdleWarmer(); }

void StoreScene::OnPause() {
  // Stop everything when covered by another scene (Settings, Version, etc.)
  // Downloads and converter will pick up incomplete items next time we resume.
  StoreManager::StopIdleWarmer();
  mImageDownloader->CancelAll();
  mIdleFrames = 0;
}

void StoreScene::OnResume() {
  mMetaReady = MetaCache::IsReady();
  mIdleFrames = 0;
  mLastQueueOffset = -1; // force SetVisibleQueue to fire on first frame back

  if (StoreList::WasStoreChanged()) {
    // Store changed while in Settings -- full reset, refetch everything
    StoreList::ClearStoreChangedFlag();
    mStoreIndex = 0;
    mHighlightedCategoryIndex = 0;
    mSideBarFocused = false;
    mHasSavedPosition = false;
    StoreManager::Reset();
    return;
  }

  // No store change -- just restore position, no purging
  if (mHasSavedPosition) {
    StoreManager::LoadAtOffset(mSavedWindowOffset);
    int32_t maxIndex = StoreManager::GetWindowStoreItemOffset() +
                       StoreManager::GetWindowStoreItemCount() - 1;
    if (maxIndex < 0) maxIndex = 0;
    mStoreIndex = mSavedStoreIndex > maxIndex ? maxIndex : mSavedStoreIndex;
    mHasSavedPosition = false;
  }
}

void StoreScene::RenderHeader() {
  float screenW = (float)Context::GetScreenWidth();

  Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0,
      screenW, ASSET_HEADER_HEIGHT);
  Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
  Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0xFFFFFFFF, 16, 12,
      ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);

  // Version -- sit beside the title aligned to top of FONT_LARGE
  float titleW = 0.0f;
  Font::MeasureText(FONT_LARGE, "Xbox Homebrew Store", &titleW);
  Font::DrawText(FONT_NORMAL, APP_VERSION, COLOR_TEXT_GRAY,
      60.0f + titleW + 10.0f, 20.0f);

  // Idle warmer indicator -- shown while running or covers still downloading
  if (StoreManager::IsIdleWarmerRunning() || StoreManager::IsIdleWarmerDownloading()) {
    float versionW = 0.0f;
    Font::MeasureText(FONT_NORMAL, APP_VERSION, &versionW);
    int32_t cached = ImageDownloader::GetCachedCoverCount();
    int32_t total = StoreManager::GetSelectedCategoryTotal();
    std::string cachingStr = String::Format("Caching... %d/%d", cached, total);
    Font::DrawText(FONT_NORMAL, cachingStr.c_str(), 0xFF88CC88,
        60.0f + titleW + 10.0f + versionW + 10.0f, 20.0f);
  }

  // FTP IP top-right
  std::string ipStr = std::string("FTP: ") + Network::GetIP();
  float ipW = 0.0f;
  Font::MeasureText(FONT_NORMAL, ipStr.c_str(), &ipW);
  Font::DrawText(FONT_NORMAL, ipStr.c_str(), COLOR_TEXT_GRAY,
      screenW - ipW - 16.0f, 20.0f);
}

void StoreScene::DrawFooterControl(float &x, float footerY,
    const char *iconName, const char *label) {
  const float iconW = ASSET_CONTROLLER_ICON_WIDTH;
  const float iconH = ASSET_CONTROLLER_ICON_HEIGHT;
  const float gap = 4.0f;
  const float groupSpacing = 20.0f;
  float textY = footerY + 12.0f;
  float iconY = footerY + 10.0f;
  float textWidth = 0.0f;

  D3DTexture *icon = TextureHelper::GetControllerIcon(iconName);
  if (icon != NULL) {
    Drawing::DrawTexturedRect(icon, 0xffffffff, x, iconY, iconW, iconH);
    x += iconW + gap;
  }
  Font::DrawText(FONT_NORMAL, label, COLOR_WHITE, x, textY);
  Font::MeasureText(FONT_NORMAL, label, &textWidth);
  x += textWidth + groupSpacing;
}

void StoreScene::RenderFooter() {
  float footerY = Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
  float x = 16.0f;

  Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0, footerY,
      Context::GetScreenWidth(), ASSET_FOOTER_HEIGHT);

  DrawFooterControl(x, footerY, "ButtonA", "Select");
  DrawFooterControl(x, footerY, "Dpad", "Navigate");
  DrawFooterControl(x, footerY, "ButtonWhite", "First");
  DrawFooterControl(x, footerY, "ButtonBlack", "Last");
  if (MetaCache::IsFailed()) {
    DrawFooterControl(x, footerY, "ButtonY", "Retry");
  }
  DrawFooterControl(x, footerY, "Start", "Settings");
}

void StoreScene::RenderCategorySidebar() {
  float sidebarHeight =
      (Context::GetScreenHeight() - ASSET_SIDEBAR_Y) - ASSET_FOOTER_HEIGHT;
  Drawing::DrawTexturedRect(TextureHelper::GetSidebar(), 0xffffffff, 0,
      ASSET_SIDEBAR_Y, ASSET_SIDEBAR_WIDTH,
      sidebarHeight);

  Font::DrawText(FONT_NORMAL, "Categories...", COLOR_WHITE, 16,
      ASSET_SIDEBAR_Y + 16);

  int32_t categoryCount = StoreManager::GetCategoryCount();

  int32_t maxItems = (int32_t)(sidebarHeight - 64) / 44;
  int32_t start = 0;
  if (categoryCount >= maxItems) {
    start = Math::ClampInt32(mHighlightedCategoryIndex - (maxItems / 2), 0,
        categoryCount - maxItems);
  }

  int32_t itemCount = Math::MinInt32(start + maxItems, categoryCount) - start;

  for (int32_t pass = 0; pass < 2; pass++) {
    float y = ASSET_SIDEBAR_Y + 64;

    if (pass == 1) {
      Drawing::BeginStencil(48.0f, (float)ASSET_SIDEBAR_Y,
          ASSET_SIDEBAR_WIDTH - 64.0f, (float)sidebarHeight);
    }

    for (int32_t i = 0; i < itemCount; i++) {
      int32_t index = start + i;

      StoreCategory *storeCategory = StoreManager::GetStoreCategory(index);

      bool highlighted = index == mHighlightedCategoryIndex;
      bool focused = mSideBarFocused && highlighted;

      if (pass == 0) {
        if (highlighted || focused) {
          Drawing::DrawTexturedRect(
              TextureHelper::GetCategoryHighlight(),
              focused ? COLOR_FOCUS_HIGHLIGHT : COLOR_HIGHLIGHT, 0, y - 32,
              ASSET_SIDEBAR_HIGHLIGHT_WIDTH, ASSET_SIDEBAR_HIGHLIGHT_HEIGHT);
        }

        bool activated = index == StoreManager::GetCategoryIndex();
        if (mSideBarFocused == true) {
          Drawing::DrawTexturedRect(
              TextureHelper::GetCategoryIcon(storeCategory->name),
              activated ? COLOR_FOCUS_HIGHLIGHT : 0xffffffff, 16, y - 2,
              ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        } else {
          Drawing::DrawTexturedRect(
              TextureHelper::GetCategoryIcon(storeCategory->name),
              activated ? COLOR_HIGHLIGHT : 0xffffffff, 16, y - 2,
              ASSET_CATEGORY_ICON_WIDTH, ASSET_CATEGORY_ICON_HEIGHT);
        }
      } else {
        if (focused == true) {
          Font::DrawTextScrolling(FONT_NORMAL, storeCategory->name, COLOR_WHITE,
              48, y, ASSET_SIDEBAR_WIDTH - 64,
              storeCategory->nameScrollState);
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

void StoreScene::DrawStoreItem(StoreItem *storeItem, float x, float y,
    bool selected, int32_t slotIndex) {
  Drawing::DrawTexturedRect(TextureHelper::GetCard(), 0xFFFFFFFF, x, y,
      ASSET_CARD_WIDTH, ASSET_CARD_HEIGHT);

  if (selected == true) {
    Drawing::DrawTexturedRect(
        TextureHelper::GetCardHighlight(),
        mSideBarFocused ? COLOR_HIGHLIGHT : COLOR_FOCUS_HIGHLIGHT, x - 3, y - 3,
        ASSET_CARD_HIGHLIGHT_WIDTH, ASSET_CARD_HIGHLIGHT_HEIGHT);
  }

  // 144/204

  float iconW = ASSET_CARD_WIDTH - 18;
  float iconH = ASSET_CARD_HEIGHT - 62;
  float iconX = x + 9;
  float iconY = y + 9;

  D3DTexture *cover = storeItem->cover;
  if (cover == NULL) {
    // Check LRU texture cache first -- avoids redundant disk loads on scroll
    cover = TextureCache::Get(storeItem->appId);
    if (cover != NULL) {
      storeItem->cover = cover;
    } else if (ImageDownloader::IsCoverCached(storeItem->appId)) {
      std::string path = ImageDownloader::GetCoverCachePath(storeItem->appId);
      bool isDxt = path.size() >= 4 && path.substr(path.size() - 4) == ".dxt";
      if (isDxt) {
        // .dxt ready -- fast memcpy upload, no stall
        D3DTexture *loaded = TextureHelper::LoadFromFile(path);
        if (loaded != NULL) {
          TextureCache::Put(storeItem->appId, loaded);
          storeItem->cover = loaded;
          cover = loaded;
        } else {
          // Load failed (shouldn't happen) -- show placeholder but DON'T
          // store it in storeItem->cover so we retry next frame
          cover = TextureHelper::GetCover();
        }
      } else {
        // .jpg present -- converter thread is working on it, show placeholder
        cover = TextureHelper::GetCover();
      }
    } else {
      // Not on disk yet -- SetVisibleQueue will handle queuing
      cover = TextureHelper::GetCover();
    }
  }
  Drawing::DrawTexturedRect(cover, 0xFFFFFFFF, iconX, iconY, iconW, iconH);

  float textX = x + 8;
  float nameY = y + iconH + 14;
  float authorY = y + iconH + 32;

  Drawing::BeginStencil((float)x + 8, (float)(y + iconH),
      (float)ASSET_CARD_WIDTH - 16, (float)62);

  if (selected && !mSideBarFocused) {
    float textMaxWidth = ASSET_CARD_WIDTH - 16;
    Font::DrawTextScrolling(FONT_NORMAL, storeItem->name, COLOR_WHITE, textX,
        nameY, textMaxWidth, storeItem->nameScrollState);
    Font::DrawTextScrolling(FONT_NORMAL, storeItem->author, COLOR_TEXT_GRAY,
        textX, authorY, textMaxWidth,
        storeItem->authorScrollState);
  } else {
    storeItem->nameScrollState.active = false;
    storeItem->authorScrollState.active = false;
    Font::DrawText(FONT_NORMAL, storeItem->name, COLOR_WHITE, textX, nameY);
    Font::DrawText(FONT_NORMAL, storeItem->author, COLOR_TEXT_GRAY, textX,
        authorY);
  }

  Drawing::EndStencil();

  if (storeItem->state == 1) {
    Drawing::DrawTexturedRect(TextureHelper::GetNewBadge(), 0xFFFFFFFF,
        x + ASSET_CARD_WIDTH - ASSET_BADGE_NEW_WIDTH + 4,
        y - 12, ASSET_BADGE_NEW_WIDTH,
        ASSET_BADGE_NEW_HEIGHT);
  } else if (storeItem->state == 2) {
    Drawing::DrawTexturedRect(
        TextureHelper::GetUpdateBadge(), 0xFFFFFFFF,
        x + ASSET_CARD_WIDTH - ASSET_BADGE_UPDATE_WIDTH + 4, y - 12,
        ASSET_BADGE_UPDATE_WIDTH, ASSET_BADGE_UPDATE_HEIGHT);
  }
}

void StoreScene::RenderMainGrid() {
  float gridX = ASSET_SIDEBAR_WIDTH;
  float gridY = ASSET_HEADER_HEIGHT;
  float gridWidth = Context::GetScreenWidth() - ASSET_SIDEBAR_WIDTH;
  float gridHeight =
      Context::GetScreenHeight() - (ASSET_HEADER_HEIGHT + ASSET_FOOTER_HEIGHT);

  int32_t count = StoreManager::GetWindowStoreItemCount();
  if (count == 0) {
    if (MetaCache::IsReady()) {
      Font::DrawText(FONT_NORMAL, "No apps in this category.",
          (uint32_t)COLOR_TEXT_GRAY, gridX, gridY);
    } else if (MetaCache::IsFailed()) {
      Font::DrawText(FONT_NORMAL, "Failed to load catalogue.",
          (uint32_t)0xFF4444FF, gridX, gridY);
    } else {
      int32_t streamed = MetaCache::GetStreamedCount();
      int32_t total    = MetaCache::GetTotalCount();
      std::string loadMsg;
      if (streamed > 0 && total > 0) {
        loadMsg = String::Format("Loading Catalogue... %d of %d items",
            streamed, total);
      } else {
        loadMsg = "Loading Catalogue...";
      }
      Font::DrawText(FONT_NORMAL, loadMsg.c_str(),
          (uint32_t)COLOR_TEXT_GRAY, gridX, gridY);
    }
    return;
  }

  float cardWidth = ASSET_CARD_WIDTH;
  float cardHeight = ASSET_CARD_HEIGHT;
  float storeItemsWidth =
      (Context::GetGridCols() * (cardWidth + CARD_GAP)) - CARD_GAP;
  float storeItemsHeight =
      (Context::GetGridRows() * (cardHeight + CARD_GAP)) - CARD_GAP;

  float cardX = gridX + ((gridWidth - storeItemsWidth) / 2);
  float cardY = gridY + ((gridHeight - storeItemsHeight) / 2);

  int32_t totalSlots = Context::GetGridCells();
  int32_t windowCount = StoreManager::GetWindowStoreItemCount();
  int32_t slotsInView = Math::MinInt32(totalSlots, windowCount);

  // Collect visible items that still need covers, call SetVisibleQueue once.
  // This replaces stale pending downloads with exactly what's on screen now,
  // and cancels any in-flight download for an item that has scrolled away.
  D3DTexture **pendingOut[32];
  std::string  pendingIds[32];
  int32_t      pendingCount = 0;

  for (int32_t currentSlot = 0; currentSlot < slotsInView; currentSlot++) {
    int32_t row = currentSlot / Context::GetGridCols();
    int32_t col = currentSlot % Context::GetGridCols();
    float x = cardX + col * (cardWidth + CARD_GAP);
    float y = cardY + row * (cardHeight + CARD_GAP);
    StoreItem *storeItem = StoreManager::GetWindowStoreItem(currentSlot);
    if (storeItem == NULL) {
      continue;
    }
    DrawStoreItem(storeItem, x, y,
        currentSlot ==
            (mStoreIndex - StoreManager::GetWindowStoreItemOffset()),
        currentSlot);

    // If cover still missing, add to pending list for SetVisibleQueue
    if (storeItem->cover == NULL && !storeItem->appId.empty() &&
        !ImageDownloader::IsCoverCached(storeItem->appId) &&
        pendingCount < 32) {
      pendingOut[pendingCount] = &storeItem->cover;
      pendingIds[pendingCount] = storeItem->appId;
      pendingCount++;
    }
  }

  // Only call SetVisibleQueue when the visible window actually changes (scroll/category).
  // Calling it every frame caused spurious cancels and double-downloads.
  int32_t currentOffset = StoreManager::GetWindowStoreItemOffset();
  if (pendingCount > 0 && currentOffset != mLastQueueOffset) {
    mLastQueueOffset = currentOffset;
    mImageDownloader->SetVisibleQueue(pendingOut, pendingIds, pendingCount);
  }

  std::string pageStr =
      String::Format("Item %d of %d", mStoreIndex + 1,
          StoreManager::GetSelectedCategoryTotal());
  float pageStrWidth = 0.0f;
  Font::MeasureText(FONT_NORMAL, pageStr, &pageStrWidth);
  float pageStrX = Context::GetScreenWidth() - 16.0f - pageStrWidth;
  Font::DrawText(FONT_NORMAL, pageStr.c_str(), (uint32_t)COLOR_TEXT_GRAY,
      pageStrX, (Context::GetScreenHeight() - 30.0f));
}

void StoreScene::Render() {
  Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0,
      Context::GetScreenWidth(),
      Context::GetScreenHeight());

  RenderHeader();
  RenderFooter();
  RenderCategorySidebar();
  RenderMainGrid();

  if (mShowInfoOverlay) {
    RenderInfoOverlay();
  }
}

void StoreScene::RenderInfoOverlay() {
  const float w = (float)Context::GetScreenWidth();
  const float h = (float)Context::GetScreenHeight();

  Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, w, h);

  const float panelW = 440.0f;
  const float panelH = 120.0f;
  float px = (w - panelW) * 0.5f;
  float py = (h - panelH) * 0.5f;

  Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
  Drawing::DrawFilledRect(0xFFEF5350, px, py, panelW, 4.0f);

  float msgMaxWidth = panelW - 40.0f;
  Font::DrawTextWrapped(FONT_NORMAL, mInfoOverlayMessage, COLOR_WHITE,
      px + 20.0f, py + 18.0f, msgMaxWidth);

  float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
  float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
  float lblW = 0.0f;
  Font::MeasureText(FONT_NORMAL, "Close", &lblW);
  float pairW = iW + 4.0f + lblW;
  float hx = px + (panelW - pairW) * 0.5f;
  float hy = py + panelH - 32.0f;
  D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
  if (iconA != NULL) {
    Drawing::DrawTexturedRect(iconA, 0xFFFFFFFF, hx, hy - 2.0f, iW, iH);
    hx += iW + 4.0f;
  }
  Font::DrawText(FONT_NORMAL, "Close", COLOR_TEXT_GRAY, hx, hy);
}

void StoreScene::Update() {
  // Detect MetaCache becoming ready -- load the window and drop into the store
  bool metaNowReady = MetaCache::IsReady();
  if (metaNowReady && !mMetaReady) {
    mMetaReady = true;
    StoreManager::RefreshApplications();
    mStoreIndex = 0;
    Debug::Print("StoreScene: MetaCache ready, window loaded\n");
  } else if (!metaNowReady) {
    mMetaReady = false; // reset when category changes and cache is purged
    if (!MetaCache::IsFailed()) {
      return; // still loading -- show indicator, block input
    }
    // Failed -- fall through so Start/Y inputs still work
  }

  if (mShowInfoOverlay) {
    if (InputManager::ControllerPressed(ControllerA, -1) ||
        InputManager::ControllerPressed(ControllerB, -1)) {
      mShowInfoOverlay = false;
    }
    return;
  }

  if (MetaCache::IsFailed() &&
      InputManager::ControllerPressed(ControllerY, -1)) {
    StoreManager::Reset();
    return;
  }

  if (InputManager::ControllerPressed(ControllerStart, -1)) {
    Context::GetSceneManager()->PushScene(new SettingsScene());
    return;
  }

  // Idle pre-cache logic -- only on All Apps (category 0)
  bool anyInput = InputManager::ControllerPressed(ControllerDpadUp, -1) ||
                  InputManager::ControllerPressed(ControllerDpadDown, -1) ||
                  InputManager::ControllerPressed(ControllerDpadLeft, -1) ||
                  InputManager::ControllerPressed(ControllerDpadRight, -1) ||
                  InputManager::ControllerPressed(ControllerA, -1) ||
                  InputManager::ControllerPressed(ControllerB, -1) ||
                  InputManager::ControllerPressed(ControllerX, -1) ||
                  InputManager::ControllerPressed(ControllerY, -1) ||
                  InputManager::ControllerPressed(ControllerStart, -1) ||
                  InputManager::ControllerPressed(ControllerBlack, -1) ||
                  InputManager::ControllerPressed(ControllerWhite, -1);
  if (anyInput) {
    if (StoreManager::IsIdleWarmerRunning() || StoreManager::IsIdleWarmerDownloading()) {
      StoreManager::StopIdleWarmer();
      Debug::Print("IdleWarmer: cancelled by input\n");
    }
    mIdleFrames = 0;
  } else if (AppSettings::GetPreCacheOnIdle() &&
             StoreManager::GetCategoryIndex() == 0) {
    bool running     = StoreManager::IsIdleWarmerRunning();
    bool downloading = StoreManager::IsIdleWarmerDownloading();
    bool queued      = StoreManager::IsWarmerQueued();
    bool done        = StoreManager::IsIdleWarmerDone();

    if (running || downloading) {
      // Warmer active -- wait, reset idle counter
      mIdleFrames = 0;
    } else if (queued && !done) {
      // Thread finished queuing and downloader just drained -- all done
      StoreManager::SetIdleWarmerDone();
      Debug::Print("IdleWarmer: all covers downloaded\n");
    } else if (!done) {
      // Idle and not started yet (or reset after cancel) -- count frames
      mIdleFrames++;
      if (mIdleFrames >= IDLE_THRESHOLD) {
        Debug::Print("IdleWarmer: idle threshold reached, starting\n");
        StoreManager::StartIdleWarmer();
        mIdleFrames = 0;
      }
    } else {
      mIdleFrames = 0;
    }
  } else {
    mIdleFrames = 0;
  }

  if (mSideBarFocused) {
    if (InputManager::ControllerPressed(ControllerDpadUp, -1)) {
      mHighlightedCategoryIndex = mHighlightedCategoryIndex > 0
                                      ? mHighlightedCategoryIndex - 1
                                      : StoreManager::GetCategoryCount() - 1;
    } else if (InputManager::ControllerPressed(ControllerDpadDown, -1)) {
      mHighlightedCategoryIndex =
          mHighlightedCategoryIndex < StoreManager::GetCategoryCount() - 1
              ? mHighlightedCategoryIndex + 1
              : 0;
    } else if (InputManager::ControllerPressed(ControllerA, -1)) {
      bool needsUpdate =
          StoreManager::GetCategoryIndex() != mHighlightedCategoryIndex;
      if (needsUpdate == true) {
        mImageDownloader->CancelAll();
        StoreManager::SetCategoryIndex(mHighlightedCategoryIndex);
        mStoreIndex = 0;
        mLastQueueOffset = -1;
      }
    } else if (InputManager::ControllerPressed(ControllerDpadRight, -1)) {
      mSideBarFocused = false;
    }
  } else {
    int32_t col = mStoreIndex % Context::GetGridCols();
    int32_t row = mStoreIndex / Context::GetGridCols();
    if (InputManager::ControllerPressed(ControllerDpadLeft, -1)) {
      if (col == 0) {
        mSideBarFocused = true;
      } else {
        mStoreIndex--;
      }
    } else if (InputManager::ControllerPressed(ControllerDpadRight, -1)) {
      if (col < (Context::GetGridCols() - 1) &&
          mStoreIndex < (StoreManager::GetSelectedCategoryTotal() - 1)) {
        mStoreIndex++;
      }
    } else if (InputManager::ControllerPressed(ControllerDpadUp, -1)) {
      if ((mStoreIndex - StoreManager::GetWindowStoreItemOffset()) >=
          Context::GetGridCols()) {
        mStoreIndex -= Context::GetGridCols();
      } else if (StoreManager::HasPrevious() && MetaCache::IsReady()) {
        StoreManager::LoadPrevious();
        mStoreIndex = mStoreIndex >= Context::GetGridCols()
                          ? mStoreIndex - Context::GetGridCols()
                          : 0;
      }
    } else if (InputManager::ControllerPressed(ControllerDpadDown, -1)) {
      if (mStoreIndex <
          (StoreManager::GetWindowStoreItemOffset() +
              StoreManager::GetWindowStoreItemCount() - Context::GetGridCols())) {
        mStoreIndex += Context::GetGridCols();
      } else if (StoreManager::HasNext() && MetaCache::IsReady()) {
        mImageDownloader->FlushQueue();
        StoreManager::LoadNext();
        mStoreIndex =
            Math::MinInt32(mStoreIndex + Context::GetGridCols(),
                StoreManager::GetSelectedCategoryTotal() - 1);
      }
    } else if (InputManager::ControllerPressed(ControllerA, -1)) {
      int32_t slotIndex =
          mStoreIndex - StoreManager::GetWindowStoreItemOffset();
      StoreItem *storeItem = StoreManager::GetWindowStoreItem(slotIndex);

      if (storeItem == NULL) {
        return;
      }

      StoreVersions storeVersions;
      ViewState::TrySave(storeItem->appId, storeItem->latestVersion);
      if (StoreManager::TryGetStoreVersions(storeItem->appId, &storeVersions)) {
        // Save position so OnResume can restore it
        mSavedStoreIndex = mStoreIndex;
        mSavedWindowOffset = StoreManager::GetWindowStoreItemOffset();
        mHasSavedPosition = true;

        SceneManager *sceneManager = Context::GetSceneManager();
        sceneManager->PushScene(new VersionScene(storeVersions));
      } else {
        mInfoOverlayMessage =
            "No downloads are available\nfor this app at this time.";
        mShowInfoOverlay = true;
      }
    } else if (InputManager::ControllerPressed(ControllerWhite, -1)) {
      // Jump to first item in category
      mImageDownloader->FlushQueue();
      StoreManager::LoadAtOffset(0);
      mStoreIndex = 0;
    } else if (InputManager::ControllerPressed(ControllerBlack, -1)) {
      // Jump to last item -- load window so last item lands on the bottom row
      int32_t total = StoreManager::GetSelectedCategoryTotal();
      if (total > 0) {
        int32_t cols = Context::GetGridCols();
        int32_t rows = Context::GetGridRows();
        int32_t cells = cols * rows;
        // Row that the last item sits on (0-based within its page)
        int32_t lastItemRow = ((total - 1) % cells) / cols;
        // Shift window back so that row lands on the bottom row of the grid
        int32_t rowShift = (rows - 1 - lastItemRow) * cols;
        int32_t windowStart = ((total - 1) / cells) * cells - rowShift;
        if (windowStart < 0) windowStart = 0;
        mImageDownloader->FlushQueue();
        StoreManager::LoadAtOffset(windowStart);
        mStoreIndex = total - 1;
      }
    }
  }
}