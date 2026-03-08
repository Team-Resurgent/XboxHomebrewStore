#include "SettingsScene.h"
#include "SceneManager.h"
#include "InstallPathScene.h"
#include "StoreManagerScene.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\String.h"
#include "..\InputManager.h"
#include "..\TextureHelper.h"
#include "..\AppSettings.h"
#include "..\StoreList.h"

// ==========================================================================
// Path browser callback
// ==========================================================================
static void OnDownloadPathConfirmed(const std::string& path, void* userData)
{
    SettingsScene* self = (SettingsScene*)userData;
    if (self != NULL)
        self->SetDownloadPath(path);
}

// ==========================================================================
// Layout constants
// ==========================================================================
static const float BODY_TOP    = ASSET_SIDEBAR_Y + 16.0f;
static const float ROW_H       = 72.0f;
static const float ROW_MARGIN  = 24.0f;
static const float SEC_GAP     = 28.0f;   // gap above each section header
static const float SEC_HEAD_H  = 26.0f;   // section label height

// ==========================================================================
// Constructor / Destructor
// ==========================================================================
SettingsScene::SettingsScene()
    : mSelectedRow(0)
    , mPickerOpen(false)
    , mPickerSel(0)
{
    mDownloadPath       = AppSettings::GetDownloadPath();
    mAfterInstallAction = AppSettings::GetAfterInstallAction();
}

SettingsScene::~SettingsScene() {}

void SettingsScene::OnResume()
{
    // Refresh store name in case StoreManagerScene changed the active store
}

// ==========================================================================
// SaveAndPop
// ==========================================================================
void SettingsScene::SaveAndPop()
{
    AppSettings::SetDownloadPath(mDownloadPath);
    AppSettings::SetAfterInstallAction(mAfterInstallAction);
    AppSettings::Save();
    Context::GetSceneManager()->PopScene();
}

// ==========================================================================
// OpenPathBrowser
// ==========================================================================
void SettingsScene::OpenPathBrowser()
{
    Context::GetSceneManager()->PushScene(
        new InstallPathScene(OnDownloadPathConfirmed, this, "Download Path"));
}

// ==========================================================================
// Update
// ==========================================================================
void SettingsScene::Update()
{
    // ---- Picker popup takes priority ----
    if (mPickerOpen)
    {
        if (InputManager::ControllerPressed(ControllerDpadUp, -1))
        {
            mPickerSel = (mPickerSel > 0) ? mPickerSel - 1 : 2;
            return;
        }
        if (InputManager::ControllerPressed(ControllerDpadDown, -1))
        {
            mPickerSel = (mPickerSel < 2) ? mPickerSel + 1 : 0;
            return;
        }
        if (InputManager::ControllerPressed(ControllerA, -1))
        {
            mAfterInstallAction = (AfterInstallAction)mPickerSel;
            mPickerOpen = false;
            return;
        }
        if (InputManager::ControllerPressed(ControllerB, -1))
        {
            mPickerOpen = false;
            return;
        }
        return;
    }

    if (InputManager::ControllerPressed(ControllerB, -1) ||
        InputManager::ControllerPressed(ControllerStart, -1))
    {
        SaveAndPop();
        return;
    }

    if (InputManager::ControllerPressed(ControllerDpadUp, -1))
    {
        mSelectedRow = (mSelectedRow > 0) ? mSelectedRow - 1 : ROW_COUNT - 1;
        return;
    }
    if (InputManager::ControllerPressed(ControllerDpadDown, -1))
    {
        mSelectedRow = (mSelectedRow < ROW_COUNT - 1) ? mSelectedRow + 1 : 0;
        return;
    }

    if (InputManager::ControllerPressed(ControllerA, -1))
    {
        switch (mSelectedRow)
        {
        case 0:
            Context::GetSceneManager()->PushScene(new StoreManagerScene());
            break;
        case 1:
            OpenPathBrowser();
            break;
        case 2:
            mPickerSel  = (int)mAfterInstallAction;
            mPickerOpen = true;
            break;
        }
        return;
    }
}

// ==========================================================================
// Render helpers
// ==========================================================================
static const char* AfterInstallLabel(AfterInstallAction a)
{
    switch (a)
    {
    case AfterInstallDelete: return "Always Delete";
    case AfterInstallKeep:   return "Always Keep";
    case AfterInstallAsk:    return "Ask Each Time";
    default:                 return "Unknown";
    }
}

// Draw a thin section divider + pink label
static void DrawSection(float x, float y, float w, const char* label)
{
    Drawing::DrawFilledRect(0x22ffffff, x, y, w, 1.0f);
    y += 5.0f;
    Font::DrawText(FONT_NORMAL, label, COLOR_FOCUS_HIGHLIGHT, x, y);
}

// Draw a two-line info row (label on top, value below, optional right hint)
static void DrawRow(float x, float y, float w, bool selected,
                    const char* label, const std::string& value,
                    const char* rightHint = NULL)
{
    uint32_t bg = selected ? 0xFF3C3C3C : 0xFF282828;
    Drawing::DrawFilledRect(bg, x, y, w, ROW_H - 2.0f);

    if (selected)
        Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 3.0f, ROW_H - 2.0f);

    float tx = x + 14.0f + (selected ? 3.0f : 0.0f);

    Font::DrawText(FONT_NORMAL, label, COLOR_TEXT_GRAY, tx, y + 8.0f);

    // Truncate value so it doesn't overlap the right hint
    float maxValW = w - 28.0f - (rightHint ? 120.0f : 0.0f);
    std::string displayVal = Font::TruncateText(FONT_NORMAL, value, maxValW);
    Font::DrawText(FONT_NORMAL, displayVal.c_str(), COLOR_WHITE, tx, y + 34.0f);

    if (rightHint != NULL)
    {
        float hw = 0.0f;
        Font::MeasureText(FONT_NORMAL, rightHint, &hw);
        uint32_t hintCol = selected ? COLOR_FOCUS_HIGHLIGHT : 0xFF666666;
        Font::DrawText(FONT_NORMAL, rightHint, hintCol,
            x + w - hw - 14.0f, y + (ROW_H - 18.0f) * 0.5f);
    }
}

// Draw a cycle row: label top, << value >> centered bottom
static void DrawCycleRow(float x, float y, float w, bool selected,
                         const char* label, const char* value)
{
    uint32_t bg = selected ? 0xFF3C3C3C : 0xFF282828;
    Drawing::DrawFilledRect(bg, x, y, w, ROW_H - 2.0f);

    if (selected)
        Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 3.0f, ROW_H - 2.0f);

    float tx = x + 14.0f + (selected ? 3.0f : 0.0f);
    Font::DrawText(FONT_NORMAL, label, COLOR_TEXT_GRAY, tx, y + 8.0f);

    if (selected)
    {
        float valW = 0.0f;
        Font::MeasureText(FONT_NORMAL, value, &valW);
        float midX = x + w * 0.5f;
        Font::DrawText(FONT_NORMAL, "<",   COLOR_FOCUS_HIGHLIGHT, midX - valW * 0.5f - 22.0f, y + 36.0f);
        Font::DrawText(FONT_NORMAL, value, COLOR_WHITE,            midX - valW * 0.5f,          y + 36.0f);
        Font::DrawText(FONT_NORMAL, ">",   COLOR_FOCUS_HIGHLIGHT,  midX + valW * 0.5f + 8.0f,   y + 36.0f);
    }
    else
    {
        Font::DrawText(FONT_NORMAL, value, COLOR_WHITE, tx, y + 36.0f);
    }
}

// ==========================================================================
// Render
// ==========================================================================
void SettingsScene::Render()
{
    float screenW = (float)Context::GetScreenWidth();
    float screenH = (float)Context::GetScreenHeight();
    float rowX    = ROW_MARGIN;
    float rowW    = screenW - ROW_MARGIN * 2.0f;
    float footerY = screenH - ASSET_FOOTER_HEIGHT;

    Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF,
        0, 0, screenW, screenH);

    // ---- Header ----
    Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff,
        0, 0, screenW, ASSET_HEADER_HEIGHT);
    Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386,
        16, 12, ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);
    Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);

    // Settings (top-right)
	const char* sub = "Settings";
    float sw = 0.0f;
    Font::MeasureText(FONT_NORMAL, sub, &sw);
    Font::DrawText(FONT_NORMAL, sub, COLOR_TEXT_GRAY, screenW - sw - 16.0f, 20.0f);

    // ---- Body ----
    float y = BODY_TOP;

    // Store section (first)
    DrawSection(rowX, y, rowW, "Store");
    y += SEC_HEAD_H;
    std::string activeName = "";
    StoreEntry* active = StoreList::GetEntry(StoreList::GetActiveIndex());
    if (active != NULL) activeName = std::string(active->name);
    std::string storeCount   = String::Format("%d configured", StoreList::GetCount());
    std::string storeDisplay = activeName + "  (" + storeCount + ")";
    DrawRow(rowX, y, rowW, mSelectedRow == 0,
            "Active Store",
            storeDisplay,
            mSelectedRow == 0 ? "[ Manage ]" : NULL);
    y += ROW_H + 2.0f;

    // Storage section (second)
    y += SEC_GAP;
    DrawSection(rowX, y, rowW, "Storage");
    y += SEC_HEAD_H;
    DrawRow(rowX, y, rowW, mSelectedRow == 1,
            "Download Path", mDownloadPath,
            mSelectedRow == 1 ? "[ Browse ]" : NULL);
    y += ROW_H + 2.0f;

    // Install Behaviour section (third)
    y += SEC_GAP;
    DrawSection(rowX, y, rowW, "Install Behaviour");
    y += SEC_HEAD_H;
    DrawRow(rowX, y, rowW, mSelectedRow == 2,
            "Keep Downloaded Files",
            AfterInstallLabel(mAfterInstallAction),
            mSelectedRow == 2 ? "[ Change ]" : NULL);
    y += ROW_H + 2.0f;

    // Unsaved changes hint (above footer)
    bool dirty = (mDownloadPath != AppSettings::GetDownloadPath()) ||
                 ((int)mAfterInstallAction != (int)AppSettings::GetAfterInstallAction());
    if (dirty)
    {
        const char* hint = "* Unsaved changes";
        float hw = 0.0f;
        Font::MeasureText(FONT_NORMAL, hint, &hw);
        Font::DrawText(FONT_NORMAL, hint, COLOR_FOCUS_HIGHLIGHT,
            screenW - hw - 14.0f, footerY - 22.0f);
    }

    // ---- Footer ----
    Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff,
        0.0f, footerY, screenW, ASSET_FOOTER_HEIGHT);

    float fx = 16.0f;
    float tw = 0.0f;
    D3DTexture* ftex = NULL;

#define DRAW_CTRL(iconName, lblStr) \
    ftex = TextureHelper::GetControllerIcon(iconName); \
    if (ftex) { \
        Drawing::DrawTexturedRect(ftex, 0xffffffff, fx, footerY + 10.0f, \
            ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT); \
        fx += ASSET_CONTROLLER_ICON_WIDTH + 4.0f; \
    } \
    Font::DrawText(FONT_NORMAL, lblStr, COLOR_WHITE, fx, footerY + 12.0f); \
    Font::MeasureText(FONT_NORMAL, lblStr, &tw); \
    fx += tw + 20.0f;

    if (mSelectedRow == 2)
    {
        DRAW_CTRL("ButtonA", "Change");
    }
    else
    {
        DRAW_CTRL("ButtonA", mSelectedRow == 0 ? "Manage Stores" : "Browse");
    }
    DRAW_CTRL("Dpad",    "Navigate");
    DRAW_CTRL("ButtonB", "Back");

#undef DRAW_CTRL

    if (mPickerOpen) RenderPicker();
}

// ==========================================================================
// RenderPicker  —  small centred popup with 3 options
// ==========================================================================
void SettingsScene::RenderPicker()
{
    float screenW = (float)Context::GetScreenWidth();
    float screenH = (float)Context::GetScreenHeight();

    Drawing::DrawFilledRect(0xBB000000, 0.0f, 0.0f, screenW, screenH);

    const char* opts[3] = { "Always Delete", "Always Keep", "Ask Each Time" };
    const float ITEM_H  = 40.0f;
    const float PAD     = 16.0f;
    float panelW = 280.0f;
    float panelH = PAD + (float)3 * ITEM_H + PAD + 32.0f;  // extra room for hints
    float px     = (screenW - panelW) * 0.5f;
    float py     = (screenH - panelH) * 0.5f;

    Drawing::DrawFilledRect(0xFF1E1E1E, px, py, panelW, panelH);
    Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, py, panelW, 3.0f);

    int i;
    for (i = 0; i < 3; i++)
    {
        float iy  = py + PAD + (float)i * ITEM_H;
        bool  sel = (mPickerSel == i);
        bool  cur = ((int)mAfterInstallAction == i);

        if (sel)
            Drawing::DrawFilledRect(0xFF2E2E2E, px, iy, panelW, ITEM_H - 2.0f);
        if (sel)
            Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, px, iy, 3.0f, ITEM_H - 2.0f);

        uint32_t col = sel ? COLOR_WHITE : COLOR_TEXT_GRAY;
        Font::DrawText(FONT_NORMAL, opts[i], col,
            (int)(px + 16.0f), (int)(iy + 11.0f));

        if (cur)
            Font::DrawText(FONT_LARGE, "*", COLOR_FOCUS_HIGHLIGHT,
                (int)(px + panelW - 26.0f), (int)(iy + 6.0f));
    }

    // Hints centred at bottom of panel
    float iW   = (float)ASSET_CONTROLLER_ICON_WIDTH;
    float iH   = (float)ASSET_CONTROLLER_ICON_HEIGHT;
    float selW = 0.0f;
    float canW = 0.0f;
    Font::MeasureText(FONT_NORMAL, "Select", &selW);
    Font::MeasureText(FONT_NORMAL, "Cancel", &canW);
    float hintW = (iW + 4.0f + selW) + 16.0f + (iW + 4.0f + canW);
    float hx    = px + (panelW - hintW) * 0.5f;
    float hy    = py + panelH - iH - 8.0f;

    D3DTexture* iconA = TextureHelper::GetControllerIcon("ButtonA");
    if (iconA) { Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hy, iW, iH); hx += iW + 4.0f; }
    Font::DrawText(FONT_NORMAL, "Select", COLOR_TEXT_GRAY, (int)hx, (int)(hy + 2.0f));
    hx += selW + 16.0f;
    D3DTexture* iconB = TextureHelper::GetControllerIcon("ButtonB");
    if (iconB) { Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hy, iW, iH); hx += iW + 4.0f; }
    Font::DrawText(FONT_NORMAL, "Cancel", COLOR_TEXT_GRAY, (int)hx, (int)(hy + 2.0f));
}