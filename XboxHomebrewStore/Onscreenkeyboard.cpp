#include "OnScreenKeyboard.h"

#include "Context.h"
#include "Defines.h"
#include "Drawing.h"
#include "Font.h"
#include "InputManager.h"
#include "TextureHelper.h"
#include "Math.h"
#include "String.h"

// ==========================================================================
// Visual constants
// ==========================================================================
const float OnScreenKeyboard::PANEL_W = 640.0f;
const float OnScreenKeyboard::PANEL_H = 320.0f;
const float OnScreenKeyboard::KEY_W   =  44.0f;
const float OnScreenKeyboard::KEY_H   =  36.0f;
const float OnScreenKeyboard::KEY_GAP =   4.0f;
const float OnScreenKeyboard::MARGIN  =  16.0f;
const float OnScreenKeyboard::INPUT_H =  36.0f;

// ==========================================================================
// Keyboard layout
// Special tokens: "BS" = backspace, "SPC" = space, "SFT" = shift,
//                 "CLR" = clear all,  "OK"  = confirm
// ==========================================================================
const char* OnScreenKeyboard::KB_ROW_LABELS[KB_ROWS][14] =
{
    { "1","2","3","4","5","6","7","8","9","0","-","_", nullptr },  // 12 keys
    { "Q","W","E","R","T","Y","U","I","O","P", nullptr },           // 10
    { "A","S","D","F","G","H","J","K","L", nullptr },               //  9
    { "Z","X","C","V","B","N","M","BS","CLR", nullptr },            //  9
    { "SFT","SPC","OK", nullptr }                                   //  3
};
const int OnScreenKeyboard::KB_ROW_LEN[KB_ROWS] = { 12, 10, 9, 9, 3 };

// ==========================================================================
// Constructor
// ==========================================================================
OnScreenKeyboard::OnScreenKeyboard()
    : mOpen(false)
    , mConfirmed(false)
    , mCancelled(false)
    , mMaxLength(64)
    , mFilter(KbFilterNone)
    , mAccentColor(0xFFDF4088)
    , mLength(0)
    , mRow(0)
    , mCol(0)
    , mShift(true)
{
    memset(mBuffer, 0, sizeof(mBuffer));
}

// ==========================================================================
// Open
// ==========================================================================
void OnScreenKeyboard::Open(const std::string& title,
                             const std::string& prefill,
                             int32_t            maxLength,
                             KbFilter           filter,
                             uint32_t           accentColor)
{
    Reset();

    mTitle       = title;
    mMaxLength   = (maxLength > 0 && maxLength < 127) ? maxLength : 64;
    mFilter      = filter;
    mAccentColor = accentColor;

    // Pre-fill buffer, respecting maxLength and filter
    for (size_t i = 0; i < prefill.size() && mLength < mMaxLength; i++)
    {
        char c = prefill[i];
        if (IsCharAllowed(c))
            mBuffer[mLength++] = c;
    }
    mBuffer[mLength] = '\0';

    mOpen = true;
}

// ==========================================================================
// Reset
// ==========================================================================
void OnScreenKeyboard::Reset()
{
    mOpen      = false;
    mConfirmed = false;
    mCancelled = false;
    mRow       = 0;
    mCol       = 0;
    mShift     = true;
    mLength    = 0;
    memset(mBuffer, 0, sizeof(mBuffer));
}

// ==========================================================================
// IsCharAllowed
// ==========================================================================
bool OnScreenKeyboard::IsCharAllowed(char c) const
{
    if (mFilter == KbFilterFilename)
    {
        // Characters illegal in Xbox/Windows folder names
        const char* blocked = "\\/:*?\"<>|";
        for (int i = 0; blocked[i] != '\0'; i++)
            if (c == blocked[i]) return false;
        return true;
    }
    if (mFilter == KbFilterNumbers)
    {
        return (c >= '0' && c <= '9') || c == '.' || c == '-';
    }
    return true; // KbFilterNone
}

// ==========================================================================
// Close (internal)
// ==========================================================================
void OnScreenKeyboard::Close(bool confirm)
{
    mConfirmed = confirm && (mLength > 0);
    mCancelled = !confirm;
    mOpen      = false;
}

// ==========================================================================
// MoveCursor
// ==========================================================================
void OnScreenKeyboard::MoveCursor(int dx, int dy)
{
    int newRow = mRow + dy;
    if (newRow < 0)        newRow = KB_ROWS - 1;
    if (newRow >= KB_ROWS) newRow = 0;

    int newCol = mCol;
    if (dy != 0)
    {
        // Keep column proportional when changing rows
        float ratio = (KB_ROW_LEN[mRow] > 1)
            ? (float)mCol / (float)(KB_ROW_LEN[mRow] - 1) : 0.0f;
        newCol = (int)(ratio * (float)(KB_ROW_LEN[newRow] - 1) + 0.5f);
    }
    else
    {
        newCol += dx;
    }

    if (newCol < 0)                    newCol = KB_ROW_LEN[newRow] - 1;
    if (newCol >= KB_ROW_LEN[newRow])  newCol = 0;

    mRow = newRow;
    mCol = newCol;
}

// ==========================================================================
// TypeKey – act on the currently selected key
// ==========================================================================
void OnScreenKeyboard::TypeKey()
{
    const char* lbl = KB_ROW_LABELS[mRow][mCol];
    if (lbl == nullptr) return;

    if (strcmp(lbl, "OK") == 0)
    {
        Close(true);
        return;
    }
    if (strcmp(lbl, "BS") == 0)
    {
        if (mLength > 0) mBuffer[--mLength] = '\0';
        return;
    }
    if (strcmp(lbl, "CLR") == 0)
    {
        memset(mBuffer, 0, sizeof(mBuffer));
        mLength = 0;
        return;
    }
    if (strcmp(lbl, "SFT") == 0)
    {
        mShift = !mShift;
        return;
    }
    if (strcmp(lbl, "SPC") == 0)
    {
        if (mLength < mMaxLength && IsCharAllowed(' '))
            mBuffer[mLength++] = ' ';
        return;
    }

    // Regular single character
    if (mLength < mMaxLength)
    {
        char c = lbl[0];
        if (c >= 'A' && c <= 'Z' && !mShift) c += 32;
        if (IsCharAllowed(c))
        {
            mBuffer[mLength++] = c;
            mBuffer[mLength]   = '\0';
        }
    }
}

// ==========================================================================
// Update
// ==========================================================================
void OnScreenKeyboard::Update()
{
    if (!mOpen) return;

    if (InputManager::ControllerPressed(ControllerB, -1)) { Close(false);          return; }
    if (InputManager::ControllerPressed(ControllerA, -1)) { TypeKey();             return; }
    if (InputManager::ControllerPressed(ControllerY, -1)) { mShift = !mShift;      return; }

    if (InputManager::ControllerPressed(ControllerX, -1))
    {
        if (mLength > 0) mBuffer[--mLength] = '\0';
        return;
    }

    if (InputManager::ControllerPressed(ControllerDpadUp,    -1)) { MoveCursor( 0, -1); return; }
    if (InputManager::ControllerPressed(ControllerDpadDown,  -1)) { MoveCursor( 0,  1); return; }
    if (InputManager::ControllerPressed(ControllerDpadLeft,  -1)) { MoveCursor(-1,  0); return; }
    if (InputManager::ControllerPressed(ControllerDpadRight, -1)) { MoveCursor( 1,  0); return; }
}

// ==========================================================================
// Render
// ==========================================================================
void OnScreenKeyboard::Render()
{
    if (!mOpen) return;

    const float screenW = (float)Context::GetScreenWidth();
    const float screenH = (float)Context::GetScreenHeight();

    // Dim background
    Drawing::DrawFilledRect(0xCC000000, 0.0f, 0.0f, screenW, screenH);

    // Panel
    float px = (screenW - PANEL_W) * 0.5f;
    float py = (screenH - PANEL_H) * 0.5f;
    Drawing::DrawFilledRect(COLOR_CARD_BG, px, py, PANEL_W, PANEL_H);

    // Accent bar at top
    Drawing::DrawFilledRect(mAccentColor, px, py, PANEL_W, 4.0f);

    // Title
    Font::DrawText(FONT_NORMAL, mTitle.c_str(), COLOR_WHITE,
                   (int)(px + MARGIN), (int)(py + 12.0f));

    // Character counter (right-aligned in title row)
    std::string counter = String::Format("%d / %d", mLength, mMaxLength);
    float counterW = 0.0f;
    Font::MeasureText(FONT_NORMAL, counter, &counterW);
    Font::DrawText(FONT_NORMAL, counter.c_str(), COLOR_TEXT_GRAY,
                   (int)(px + PANEL_W - MARGIN - counterW), (int)(py + 12.0f));

    // Input box
    float inputX = px + MARGIN;
    float inputY = py + 36.0f;
    float inputW = PANEL_W - MARGIN * 2.0f;

    Drawing::DrawFilledRect(COLOR_SECONDARY, inputX, inputY, inputW, INPUT_H);
    Drawing::DrawFilledRect(mAccentColor,
                            inputX, inputY + INPUT_H - 2.0f, inputW, 2.0f);

    std::string display = std::string(mBuffer) + "|";
    Drawing::BeginStencil(inputX + 4.0f, inputY, inputW - 8.0f, INPUT_H);
    Font::DrawText(FONT_NORMAL, display.c_str(), COLOR_WHITE,
                   (int)(inputX + 8.0f), (int)(inputY + 9.0f));
    Drawing::EndStencil();

    // Keys
    float keysTop = inputY + INPUT_H + MARGIN;

    for (int row = 0; row < KB_ROWS; row++)
    {
        int rowLen = KB_ROW_LEN[row];

        // Measure total row width to centre it
        float rowTotalW = 0.0f;
        for (int col = 0; col < rowLen; col++)
        {
            const char* lbl = KB_ROW_LABELS[row][col];
            if (!lbl) break;
            bool wide = (strcmp(lbl,"SPC")==0 || strcmp(lbl,"SFT")==0
                      || strcmp(lbl,"OK") ==0 || strcmp(lbl,"CLR")==0);
            rowTotalW += (wide ? KEY_W * 2.2f : KEY_W) + KEY_GAP;
        }
        rowTotalW -= KEY_GAP;

        float rowX = px + (PANEL_W - rowTotalW) * 0.5f;
        float rowY = keysTop + row * (KEY_H + KEY_GAP);

        for (int col = 0; col < rowLen; col++)
        {
            const char* lbl = KB_ROW_LABELS[row][col];
            if (!lbl) break;

            bool wide  = (strcmp(lbl,"SPC")==0 || strcmp(lbl,"SFT")==0
                       || strcmp(lbl,"OK") ==0 || strcmp(lbl,"CLR")==0);
            float keyW = wide ? KEY_W * 2.2f : KEY_W;
            bool  sel  = (row == mRow && col == mCol);

            // Key background
            uint32_t bg = sel ? mAccentColor : COLOR_SECONDARY;
            if (strcmp(lbl,"OK")  == 0) bg = sel ? 0xFF66BB6A : 0xFF2E7D32;
            if (strcmp(lbl,"BS")  == 0 ||
                strcmp(lbl,"CLR") == 0) bg = sel ? 0xFFEF5350 : 0xFF7B1A1A;

            Drawing::DrawFilledRect(bg, rowX, rowY, keyW, KEY_H);

            // Key label
            const char* renderLbl = lbl;
            char singleBuf[3] = { 0, 0, 0 };
            if (strlen(lbl) == 1 && lbl[0] >= 'A' && lbl[0] <= 'Z' && !mShift)
            {
                singleBuf[0] = lbl[0] + 32;
                renderLbl    = singleBuf;
            }
            if (strcmp(lbl,"SFT") == 0) renderLbl = mShift ? "SHIFT" : "shift";
            if (strcmp(lbl,"SPC") == 0) renderLbl = "SPACE";
            if (strcmp(lbl,"BS")  == 0) renderLbl = "<-";

            float lblW = 0.0f;
            Font::MeasureText(FONT_NORMAL, renderLbl, &lblW);
            Font::DrawText(FONT_NORMAL, renderLbl, COLOR_WHITE,
                           (int)(rowX + (keyW - lblW) * 0.5f),
                           (int)(rowY + (KEY_H - 16.0f) * 0.5f));

            rowX += keyW + KEY_GAP;
        }
    }

    // Footer hint bar at panel bottom
    float hintY = py + PANEL_H - 28.0f;
    float hx    = px + MARGIN;

    D3DTexture* iconA = TextureHelper::GetControllerIcon("ButtonA");
    if (iconA) { Drawing::DrawTexturedRect(iconA, 0xffffffff, hx, hintY, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT); hx += ASSET_CONTROLLER_ICON_WIDTH + 4.0f; }
    Font::DrawText(FONT_NORMAL, "Type", COLOR_WHITE, (int)hx, (int)hintY); hx += 48.0f;

    D3DTexture* iconX = TextureHelper::GetControllerIcon("ButtonX");
    if (iconX) { Drawing::DrawTexturedRect(iconX, 0xffffffff, hx, hintY, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT); hx += ASSET_CONTROLLER_ICON_WIDTH + 4.0f; }
    Font::DrawText(FONT_NORMAL, "Backspace", COLOR_WHITE, (int)hx, (int)hintY); hx += 104.0f;

    D3DTexture* iconY = TextureHelper::GetControllerIcon("ButtonY");
    if (iconY) { Drawing::DrawTexturedRect(iconY, 0xffffffff, hx, hintY, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT); hx += ASSET_CONTROLLER_ICON_WIDTH + 4.0f; }
    Font::DrawText(FONT_NORMAL, "Shift", COLOR_WHITE, (int)hx, (int)hintY); hx += 60.0f;

    D3DTexture* iconB = TextureHelper::GetControllerIcon("ButtonB");
    if (iconB) { Drawing::DrawTexturedRect(iconB, 0xffffffff, hx, hintY, ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT); hx += ASSET_CONTROLLER_ICON_WIDTH + 4.0f; }
    Font::DrawText(FONT_NORMAL, "Cancel", COLOR_WHITE, (int)hx, (int)hintY);
}