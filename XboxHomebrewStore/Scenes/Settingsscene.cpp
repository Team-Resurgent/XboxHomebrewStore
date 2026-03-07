#include "SettingsScene.h"
#include "SceneManager.h"
#include "InstallPathScene.h"

#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\String.h"
#include "..\InputManager.h"
#include "..\TextureHelper.h"
#include "..\AppSettings.h"

// ==========================================================================
// Path browser callback
// ==========================================================================
static void OnDownloadPathConfirmed(const std::string& path, void* userData)
{
    SettingsScene* self = (SettingsScene*)userData;
    if (self != nullptr)
        self->SetDownloadPath(path);
}

// ==========================================================================
// Layout constants
// ==========================================================================
static const float ROW_H       = 72.0f;
static const float ROW_PADDING = 16.0f;
static const float ROW_START_Y = ASSET_SIDEBAR_Y + 24.0f;

// ==========================================================================
// Constructor / Destructor
// ==========================================================================
SettingsScene::SettingsScene()
    : mSelectedRow(0)
{
    mDownloadPath       = AppSettings::GetDownloadPath();
    mAfterInstallAction = AppSettings::GetAfterInstallAction();
}

SettingsScene::~SettingsScene()
{
}

// ==========================================================================
// OnResume – called after InstallPathScene pops back
// ==========================================================================
void SettingsScene::OnResume()
{
    // Download path is updated via the callback before OnResume fires
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
// CycleAfterInstall
// ==========================================================================
void SettingsScene::CycleAfterInstall(int dir)
{
    int v = (int)mAfterInstallAction + dir;
    if (v < 0) v = 2;
    if (v > 2) v = 0;
    mAfterInstallAction = (AfterInstallAction)v;
}

// ==========================================================================
// Update
// ==========================================================================
void SettingsScene::Update()
{
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
        if (mSelectedRow == 0)
            OpenPathBrowser();
        else if (mSelectedRow == 1)
            CycleAfterInstall(1);
        return;
    }

    if (InputManager::ControllerPressed(ControllerDpadLeft, -1))
    {
        if (mSelectedRow == 1) CycleAfterInstall(-1);
        return;
    }
    if (InputManager::ControllerPressed(ControllerDpadRight, -1))
    {
        if (mSelectedRow == 1) CycleAfterInstall(1);
        return;
    }
}

// ==========================================================================
// Render
// ==========================================================================
void SettingsScene::Render()
{
    Drawing::DrawTexturedRect(TextureHelper::GetBackground(), 0xFFFFFFFF,
        0, 0, (float)Context::GetScreenWidth(), (float)Context::GetScreenHeight());

    RenderHeader();
    RenderFooter();
    RenderRows();
}

// --------------------------------------------------------------------------
void SettingsScene::RenderHeader()
{
    Drawing::DrawTexturedRect(TextureHelper::GetHeader(), 0xffffffff,
        0, 0, (float)Context::GetScreenWidth(), ASSET_HEADER_HEIGHT);
    Font::DrawText(FONT_LARGE, "Xbox Homebrew Store", COLOR_WHITE, 60, 12);
    Drawing::DrawTexturedRect(TextureHelper::GetStore(), 0x8fe386,
        16, 12, ASSET_STORE_ICON_WIDTH, ASSET_STORE_ICON_HEIGHT);

    float lblW = 0.0f;
    Font::MeasureText(FONT_NORMAL, "Settings", &lblW);
    Font::DrawText(FONT_NORMAL, "Settings", COLOR_TEXT_GRAY,
        (float)Context::GetScreenWidth() - lblW - 16.0f, 20.0f);
}

// --------------------------------------------------------------------------
void SettingsScene::DrawFooterControl(float& x, float footerY,
                                       const char* iconName, const char* label)
{
    float textWidth = 0.0f;
    D3DTexture* icon = TextureHelper::GetControllerIcon(iconName);
    if (icon != nullptr)
    {
        Drawing::DrawTexturedRect(icon, 0xffffffff, x, footerY + 10.0f,
            ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        x += ASSET_CONTROLLER_ICON_WIDTH + 4.0f;
    }
    Font::DrawText(FONT_NORMAL, label, COLOR_WHITE, x, footerY + 12.0f);
    Font::MeasureText(FONT_NORMAL, label, &textWidth);
    x += textWidth + 20.0f;
}

// --------------------------------------------------------------------------
void SettingsScene::RenderFooter()
{
    float footerY = (float)Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT;
    float x = 16.0f;

    Drawing::DrawTexturedRect(TextureHelper::GetFooter(), 0xffffffff,
        0.0f, footerY, (float)Context::GetScreenWidth(), ASSET_FOOTER_HEIGHT);

    DrawFooterControl(x, footerY, "ButtonA",  "Change");
    DrawFooterControl(x, footerY, "Dpad",     "Navigate");
    DrawFooterControl(x, footerY, "ButtonB",  "Save & Back");
}

// --------------------------------------------------------------------------
void SettingsScene::RenderPathRow(float x, float y, float w,
                                   bool selected, const char* label,
                                   const std::string& value)
{
    uint32_t bg = selected ? COLOR_CARD_SEL : COLOR_CARD_BG;
    Drawing::DrawFilledRect(bg, x, y, w, ROW_H - 4.0f);

    if (selected)
        Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 4.0f, ROW_H - 4.0f);

    float textX = x + ROW_PADDING + (selected ? 6.0f : 2.0f);

    Font::DrawText(FONT_NORMAL, label, COLOR_TEXT_GRAY, textX, y + 10.0f);

    Drawing::BeginStencil(textX, y + 30.0f, w - ROW_PADDING * 2.0f - 8.0f, 28.0f);
    Font::DrawText(FONT_NORMAL, value.c_str(), COLOR_WHITE, textX, y + 32.0f);
    Drawing::EndStencil();

    if (selected)
    {
        float hintW = 0.0f;
        Font::MeasureText(FONT_NORMAL, "Browse", &hintW);
        Font::DrawText(FONT_NORMAL, "Browse", COLOR_FOCUS_HIGHLIGHT,
            x + w - hintW - ROW_PADDING, y + 24.0f);
    }
}

// --------------------------------------------------------------------------
void SettingsScene::RenderCycleRow(float x, float y, float w,
                                    bool selected, const char* label,
                                    const char* value)
{
    uint32_t bg = selected ? COLOR_CARD_SEL : COLOR_CARD_BG;
    Drawing::DrawFilledRect(bg, x, y, w, ROW_H - 4.0f);

    if (selected)
        Drawing::DrawFilledRect(COLOR_FOCUS_HIGHLIGHT, x, y, 4.0f, ROW_H - 4.0f);

    float textX = x + ROW_PADDING + (selected ? 6.0f : 2.0f);

    Font::DrawText(FONT_NORMAL, label, COLOR_TEXT_GRAY, textX, y + 10.0f);

    if (selected)
    {
        float valW = 0.0f;
        Font::MeasureText(FONT_NORMAL, value, &valW);
        float midX = x + w * 0.5f;
        Font::DrawText(FONT_NORMAL, "<", COLOR_FOCUS_HIGHLIGHT, midX - valW * 0.5f - 22.0f, y + 32.0f);
        Font::DrawText(FONT_NORMAL, value, COLOR_WHITE,          midX - valW * 0.5f,          y + 32.0f);
        Font::DrawText(FONT_NORMAL, ">", COLOR_FOCUS_HIGHLIGHT,  midX + valW * 0.5f + 8.0f,   y + 32.0f);
    }
    else
    {
        Font::DrawText(FONT_NORMAL, value, COLOR_WHITE, textX, y + 32.0f);
    }
}

// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
void SettingsScene::RenderRows()
{
    const float screenW = (float)Context::GetScreenWidth();
    const float rowW    = screenW - 48.0f;
    const float rowX    = 24.0f;
    float y = ROW_START_Y;

    // --- Storage section ---
    Font::DrawText(FONT_NORMAL, "Storage", COLOR_TEXT_GRAY, rowX, y - 18.0f);

    RenderPathRow(rowX, y, rowW, mSelectedRow == 0,
                  "Download Path", mDownloadPath);
    y += ROW_H + 16.0f;

    // --- After Install section ---
    Font::DrawText(FONT_NORMAL, "After Install", COLOR_TEXT_GRAY, rowX, y - 18.0f);

    RenderCycleRow(rowX, y, rowW, mSelectedRow == 1,
                   "Downloaded File Handling",
                   AfterInstallLabel(mAfterInstallAction));

    // Unsaved changes hint
    bool dirty = (mDownloadPath != AppSettings::GetDownloadPath()) ||
                 ((int)mAfterInstallAction != (int)AppSettings::GetAfterInstallAction());

    if (dirty)
    {
        float hintW = 0.0f;
        Font::MeasureText(FONT_NORMAL, "Unsaved changes  -  press B to save", &hintW);
        Font::DrawText(FONT_NORMAL, "Unsaved changes  -  press B to save",
            COLOR_FOCUS_HIGHLIGHT,
            screenW * 0.5f - hintW * 0.5f,
            (float)Context::GetScreenHeight() - ASSET_FOOTER_HEIGHT - 26.0f);
    }
}