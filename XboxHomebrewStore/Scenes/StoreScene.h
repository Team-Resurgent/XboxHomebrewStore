#pragma once

#include "Scene.h"

#include "..\Font.h"
#include "..\ImageDownloader.h"
#include "..\Main.h"
#include "..\StoreManager.h"

class StoreScene : public Scene {
public:
  explicit StoreScene();
  virtual ~StoreScene();
  virtual void Render();
  virtual void Update();
  virtual void OnResume();

private:
  void RenderHeader();
  void RenderFooter();
  void DrawFooterControl(float &x, float footerY, const char *iconName,
      const char *label);
  void RenderCategorySidebar();
  void RenderMainGrid();
  void DrawStoreItem(StoreItem *storeItem, float x, float y, bool selected,
      int32_t slotIndex);
  void RenderInfoOverlay();

  ImageDownloader *mImageDownloader;
  bool mSideBarFocused;
  int32_t mHighlightedCategoryIndex;
  int32_t mStoreIndex;

  // Saved position restored when returning from VersionScene
  int32_t mSavedStoreIndex;
  int32_t mSavedWindowOffset;
  bool mHasSavedPosition;

  // Info overlay (e.g. "No downloads available")
  bool mShowInfoOverlay;
  std::string mInfoOverlayMessage;

  // Idle pre-cache warmer
  int32_t mIdleFrames;                        // frames since last input
  static const int32_t IDLE_THRESHOLD = 1800; // 30s at 60fps
};
