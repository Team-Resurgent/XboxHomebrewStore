#include "StoreManagerScene.h"
#include "SceneManager.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\InputManager.h"
#include "..\StoreList.h"
#include "..\String.h"
#include "..\TextureHelper.h"

const float StoreManagerScene::ROW_H = 76.0f;

static const float LIST_TOP = ASSET_SIDEBAR_Y + 8.0f;
static const float ROW_MARGIN = 24.0f;

// ==========================================================================
StoreManagerScene::StoreManagerScene()
    : mSelectedRow(0), mScrollOffset(0), mOskStep(OskNone), mOskIsEdit(false),
      mOskEditIndex(-1), mDeleteOpen(false), mDeleteIndex(-1) {}

StoreManagerScene::~StoreManagerScene() {}
void StoreManagerScene::OnResume() {}

// ==========================================================================
// Actions
// ==========================================================================
void StoreManagerScene::SetActive(int32_t idx) {
  StoreList::SetActiveIndex(idx);
  StoreList::Save();
}

void StoreManagerScene::BeginDelete(int32_t idx) {
  if (StoreList::GetCount() <= 1) {
    return;
  }
  mDeleteIndex = idx;
  mDeleteOpen = true;
}

void StoreManagerScene::BeginAdd() {
  if (StoreList::GetCount() >= STORE_LIST_MAX) {
    return;
  }
  mOskIsEdit = false;
  mOskEditIndex = -1;
  OskOpen(OskName, "");
}

void StoreManagerScene::BeginEdit(int32_t idx) {
  StoreEntry *e = StoreList::GetEntry(idx);
  if (e == nullptr) {
    return;
  }
  mOskIsEdit = true;
  mOskEditIndex = idx;
  OskOpen(OskName, std::string(e->name));
}

// ==========================================================================
// OSK
// ==========================================================================
void StoreManagerScene::OskOpen(OskStep step, const std::string &prefill) {
  mOskStep = step;
  const char *title = (step == OskName) ? "Store Name" : "Store URL";
  int32_t maxLen = (step == OskName) ? STORE_NAME_MAX - 1 : STORE_URL_MAX - 1;
  uint32_t accent = (step == OskUrl) ? 0xFF42A5F5 : COLOR_FOCUS_HIGHLIGHT;
  mKeyboard.Open(title, prefill, maxLen, KbFilterNone, accent);
}

void StoreManagerScene::OskConfirmed(const std::string &value) {
  if (mOskStep == OskName) {
    mOskPendingName = value;
    // Now ask for URL
    std::string urlPrefill = "https://";
    if (mOskIsEdit) {
      StoreEntry *e = StoreList::GetEntry(mOskEditIndex);
      if (e != nullptr) {
        urlPrefill = std::string(e->url);
      }
    }
    OskOpen(OskUrl, urlPrefill);
    return;
  }

  if (mOskStep == OskUrl) {
    if (mOskIsEdit) {
      StoreList::Edit(mOskEditIndex, mOskPendingName, value);
    } else {
      int32_t newIdx = StoreList::Add(mOskPendingName, value);
      if (newIdx >= 0) {
        mSelectedRow = newIdx;
      }
    }
    StoreList::Save();
  }

  mOskStep = OskNone;
}

// ==========================================================================
// Update
// ==========================================================================
void StoreManagerScene::Update() {
  // OSK overlay
  if (mKeyboard.IsOpen()) {
    mKeyboard.Update();
    if (mKeyboard.IsConfirmed()) {
      OskConfirmed(mKeyboard.GetResult());
    }
    if (!mKeyboard.IsOpen() && !mKeyboard.IsConfirmed()) {
      mOskStep = OskNone;
    }
    return;
  }

  // Delete confirm dialog
  if (mDeleteOpen) {
    if (InputManager::ControllerPressed(ControllerA, -1)) {
      StoreList::Delete(mDeleteIndex);
      StoreList::Save();
      if (mSelectedRow >= StoreList::GetCount()) {
        mSelectedRow = StoreList::GetCount() - 1;
      }
      mDeleteOpen = false;
      return;
    }
    if (InputManager::ControllerPressed(ControllerB, -1)) {
      mDeleteOpen = false;
      return;
    }
    return;
  }

  // Main navigation
  if (InputManager::ControllerPressed(ControllerB, -1) ||
      InputManager::ControllerPressed(ControllerStart, -1)) {
    Context::GetSceneManager()->PopScene();
    return;
  }

  int32_t count = StoreList::GetCount();

  const int32_t VISIBLE_ROWS = 7;

  if (InputManager::ControllerPressed(ControllerDpadUp, -1)) {
    mSelectedRow = (mSelectedRow > 0) ? mSelectedRow - 1 : count - 1;
    if (mSelectedRow < mScrollOffset) {
      mScrollOffset = mSelectedRow;
    }
    if (mSelectedRow == count - 1) {
      mScrollOffset = (count > VISIBLE_ROWS) ? count - VISIBLE_ROWS : 0;
    }
    return;
  }
  if (InputManager::ControllerPressed(ControllerDpadDown, -1)) {
    mSelectedRow = (mSelectedRow < count - 1) ? mSelectedRow + 1 : 0;
    if (mSelectedRow >= mScrollOffset + VISIBLE_ROWS) {
      mScrollOffset = mSelectedRow - VISIBLE_ROWS + 1;
    }
    if (mSelectedRow == 0) {
      mScrollOffset = 0;
    }
    return;
  }

  if (InputManager::ControllerPressed(ControllerA, -1)) {
    SetActive(mSelectedRow);
    return;
  }

  if (InputManager::ControllerPressed(ControllerY, -1)) {
    BeginEdit(mSelectedRow);
    return;
  }

  if (InputManager::ControllerPressed(ControllerX, -1)) {
    BeginDelete(mSelectedRow);
    return;
  }

  if (InputManager::ControllerPressed(ControllerBlack, -1)) {
    BeginAdd();
    return;
  }
}

// ==========================================================================
// Render
// ==========================================================================
void StoreManagerScene::Render() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();

  Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF, 0, 0,
      screenW, screenH);

  RenderHeader();
  RenderFooter();
  RenderList();

  if (mKeyboard.IsOpen()) {
    mKeyboard.Render();
  }

  if (mDeleteOpen) {
    StoreEntry *e = StoreList::GetEntry(mDeleteIndex);
    std::string storeName = (e != NULL) ? std::string(e->name) : "this store";

    Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, screenW, screenH);

    float panelW = 500.0f;
    float panelH = 130.0f;
    float px = (screenW - panelW) * 0.5f;
    float py = (screenH - panelH) * 0.5f;

    Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, panelW, panelH);
    Drawing::DrawFilledRect(0xFFEF5350, px, py, panelW, 4.0f);

    Font::DrawText(FONT_NORMAL, "Delete Store?", COLOR_WHITE, (px + 16.0f),
        (py + 16.0f));

    std::string msg = String::Format(
        "'%s'  -  This cannot be undone.",
        Font::TruncateText(FONT_NORMAL, storeName, panelW - 32.0f).c_str());
    Font::DrawText(FONT_NORMAL, msg.c_str(), COLOR_TEXT_GRAY, (px + 16.0f),
        (py + 46.0f));

    float iW = (float)ASSET_CONTROLLER_ICON_WIDTH;
    float iH = (float)ASSET_CONTROLLER_ICON_HEIGHT;
    float delW = 0.0f;
    float canW = 0.0f;
    Font::MeasureText(FONT_NORMAL, "Delete", &delW);
    Font::MeasureText(FONT_NORMAL, "Cancel", &canW);
    float pairW = (iW + 4.0f + delW) + 24.0f + (iW + 4.0f + canW);
    float hx = px + (panelW - pairW) * 0.5f;
    float hy = py + panelH - 36.0f;

    D3DTexture *iconA = TextureHelper::GetControllerIcon("ButtonA");
    if (iconA) {
      Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy, iW, iH);
      hx += iW + 4.0f;
    }
    Font::DrawText(FONT_NORMAL, "Delete", 0xFFEF5350, hx, hy);
    hx += delW + 24.0f;

    D3DTexture *iconB = TextureHelper::GetControllerIcon("ButtonB");
    if (iconB) {
      Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy, iW, iH);
      hx += iW + 4.0f;
    }
    Font::DrawText(FONT_NORMAL, "Cancel", COLOR_WHITE, hx, hy);
  }
}

// --------------------------------------------------------------------------
void StoreManagerScene::RenderHeader() {
  float screenW = (float)Context::GetScreenWidth();
  Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff, 0, 0,
      screenW, ASSET_HEADER_HEIGHT);
  Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386, 16, 12,
      ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
  Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);

  const char *sub = "Manage Stores";
  float sw = 0.0f;
  Font::MeasureText(FONT_NORMAL, sub, &sw);
  Font::DrawText(FONT_NORMAL, sub, COLOR_TEXT_GRAY, screenW - sw - 16.0f,
      20.0f);
}

// --------------------------------------------------------------------------
void StoreManagerScene::RenderFooter() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();
  float footerY = screenH - ASSET_FOOTER_HEIGHT;

  Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff, 0.0f,
      footerY, screenW, ASSET_FOOTER_HEIGHT);

  float fx = 16.0f;
  float tw = 0.0f;
  D3DTexture *ftex = NULL;

#define DRAW_CTRL(iconName, lblStr)                                      \
  ftex = TextureHelper::GetControllerIcon(iconName);                     \
  if (ftex) {                                                            \
    Drawing::DrawTexturedRect(ftex, 0xffffffff, fx, footerY + 10.0f,     \
        ASSET_CONTROLLER_ICON_WIDTH,                                     \
        ASSET_CONTROLLER_ICON_HEIGHT);                                   \
    fx += ASSET_CONTROLLER_ICON_WIDTH + 4.0f;                            \
  }                                                                      \
  Font::DrawText(FONT_NORMAL, lblStr, COLOR_WHITE, fx, footerY + 12.0f); \
  Font::MeasureText(FONT_NORMAL, lblStr, &tw);                           \
  fx += tw + 20.0f;

  DRAW_CTRL("ButtonA", "Set Active");
  DRAW_CTRL("ButtonY", "Edit");
  DRAW_CTRL("ButtonX", "Delete");
  DRAW_CTRL("ButtonBlack", "Add Store");
  DRAW_CTRL("Dpad", "Navigate");
  DRAW_CTRL("ButtonB", "Back");

#undef DRAW_CTRL
}

// --------------------------------------------------------------------------
void StoreManagerScene::RenderList() {
  float screenW = (float)Context::GetScreenWidth();
  float screenH = (float)Context::GetScreenHeight();
  float footerY = screenH - ASSET_FOOTER_HEIGHT;
  float rowX = ROW_MARGIN;
  float rowW = screenW - ROW_MARGIN * 2.0f;

  int32_t count = StoreList::GetCount();

  std::string info =
      String::Format("%d store%s configured", count, count == 1 ? "" : "s");
  Font::DrawText(FONT_NORMAL, info.c_str(), COLOR_TEXT_GRAY, rowX,
      LIST_TOP - 2.0f);

  float y = LIST_TOP + 20.0f;

  for (int32_t i = 0; i < count; i++) {
    int32_t visIdx = i - mScrollOffset;
    if (visIdx < 0) {
      continue;
    }
    float rowY = y + (float)visIdx * (ROW_H + 3.0f);
    if (rowY + ROW_H > footerY) {
      break;
    }
    RenderStoreRow(rowX, rowY, rowW, mSelectedRow == i, i);
  }
}

// --------------------------------------------------------------------------
void StoreManagerScene::RenderStoreRow(float x, float y, float w, bool selected,
    int32_t idx) {
  StoreEntry *e = StoreList::GetEntry(idx);
  if (e == nullptr) {
    return;
  }

  bool isActive = (idx == StoreList::GetActiveIndex());

  uint32_t bg = selected ? 0xFF363636 : isActive ? 0xFF252535
                                                 : 0xFF222222;
  Drawing::DrawFilledRect(bg, x, y, w, ROW_H - 2.0f);

  if (selected && isActive) {
    Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 4.0f, ROW_H - 2.0f);
  } else if (selected) {
    Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 4.0f, ROW_H - 2.0f);
  } else if (isActive) {
    Drawing::DrawFilledRect(0xFF66BB6A, x, y, 4.0f, ROW_H - 2.0f);
  }

  float tx = x + 14.0f;

  std::string idxStr = String::Format("%d", idx + 1);
  float numW = 0.0f;
  Font::MeasureText(FONT_NORMAL, idxStr.c_str(), &numW);
  uint32_t circleCol =
      isActive ? 0xFF66BB6A : (selected ? COLOR_FOCUS_HIGHLIGHT : 0xFF444444);
  Drawing::DrawFilledRect(circleCol, tx, y + 22.0f, 24.0f, 24.0f);
  Font::DrawText(FONT_NORMAL, idxStr.c_str(), COLOR_WHITE,
      tx + (24.0f - numW) * 0.5f, y + 24.0f);
  tx += 34.0f;

  float nameMaxW = w - tx + x - 100.0f;
  std::string displayName =
      Font::TruncateText(FONT_NORMAL, std::string(e->name), nameMaxW);
  uint32_t nameCol = isActive ? 0xFF66BB6A : COLOR_WHITE;
  Font::DrawText(FONT_NORMAL, displayName.c_str(), nameCol, tx, y + 10.0f);

  float urlMaxW = w - tx + x - 100.0f;
  std::string displayUrl =
      Font::TruncateText(FONT_NORMAL, std::string(e->url), urlMaxW);
  Font::DrawText(FONT_NORMAL, displayUrl.c_str(), COLOR_TEXT_GRAY, tx,
      y + 34.0f);

  if (isActive) {
    const char *badgeText = "ACTIVE";
    float bw = 0.0f;
    Font::MeasureText(FONT_NORMAL, badgeText, &bw);
    float bx = x + w - bw - 20.0f;
    Drawing::DrawFilledRect(0xFF1B3A1B, bx - 8.0f, y + 22.0f, bw + 16.0f,
        24.0f);
    Font::DrawText(FONT_NORMAL, badgeText, 0xFF66BB6A, bx, y + 24.0f);
  }
}
