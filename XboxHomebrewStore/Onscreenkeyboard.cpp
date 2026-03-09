#include "OnScreenKeyboard.h"

#include "Context.h"
#include "Defines.h"
#include "Drawing.h"
#include "Font.h"
#include "InputManager.h"
#include "TextureHelper.h"
#include "String.h"

// ==========================================================================
// Dimensions
// ==========================================================================
const float OnScreenKeyboard::PANEL_W    = 680.0f;
const float OnScreenKeyboard::PANEL_H    = 332.0f;
const float OnScreenKeyboard::KEY_W      =  46.0f;
const float OnScreenKeyboard::KEY_H      =  38.0f;
const float OnScreenKeyboard::KEY_GAP    =   4.0f;
const float OnScreenKeyboard::MARGIN     =  14.0f;
const float OnScreenKeyboard::INPUT_H    =  38.0f;
const float OnScreenKeyboard::MODE_COL_W =  80.0f;  // left column for Done/Shift/Caps/#+=

// ==========================================================================
// Character grids  (ROW_COUNT x COL_COUNT)
// '\0' = empty cell (row 0 numbers are shared across modes)
// ==========================================================================

// Numbers row is mode-independent — always row 0
static const char NUMBERS[10] = { '1','2','3','4','5','6','7','8','9','0' };

const char OnScreenKeyboard::KB_ABC[ROW_COUNT][COL_COUNT] =
{
    { '1','2','3','4','5','6','7','8','9','0' },
    { 'q','w','e','r','t','y','u','i','o','p' },
    { 'a','s','d','f','g','h','j','k','l','-' },
    { 'z','x','c','v','b','n','m','_','@','.' },
};

const char OnScreenKeyboard::KB_SYM[ROW_COUNT][COL_COUNT] =
{
    { '1','2','3','4','5','6','7','8','9','0' },  // numbers stay
    { ',',';',':','\'','"','!','?','&','|','%' },
    { '[',']','{','}','\\','^','$','#','=','~' },
    { '<','>','(',')','+','-','*','/','@','.' },
};

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
    , mCaretPos(0)
    , mRow(0)
    , mCol(0)
    , mMode(KbModeAbc)
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

    for (size_t i = 0; i < prefill.size() && mLength < mMaxLength; i++)
    {
        char c = prefill[i];
        if (IsCharAllowed(c))
            mBuffer[mLength++] = c;
    }
    mBuffer[mLength] = '\0';
    mCaretPos = mLength;  // caret starts at end of prefill
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
    mCol       = 0;       // start on mode column (Done)
    mMode      = KbModeAbc;
    mLength    = 0;
    mCaretPos  = 0;
    memset(mBuffer, 0, sizeof(mBuffer));
}

// ==========================================================================
// IsCharAllowed
// ==========================================================================
bool OnScreenKeyboard::IsCharAllowed(char c) const
{
    if (mFilter == KbFilterFilename)
    {
        const char* blocked = "\\/:*?\"<>|";
        for (int i = 0; blocked[i] != '\0'; i++)
            if (c == blocked[i]) return false;
        return true;
    }
    if (mFilter == KbFilterNumbers)
        return (c >= '0' && c <= '9') || c == '.' || c == '-';
    return true;
}

// ==========================================================================
// Close
// ==========================================================================
void OnScreenKeyboard::Close(bool confirm)
{
    mConfirmed = confirm && (mLength > 0);
    mCancelled = !confirm;
    mOpen      = false;
}

// ==========================================================================
// TypeChar  –  insert character at caret position
// ==========================================================================
void OnScreenKeyboard::TypeChar(char c)
{
    if (!IsCharAllowed(c)) return;
    if (mLength >= mMaxLength) return;

    // Shift everything from caret onwards one position right
    for (int32_t i = mLength; i > mCaretPos; i--)
        mBuffer[i] = mBuffer[i - 1];

    mBuffer[mCaretPos] = c;
    mLength++;
    mCaretPos++;
    mBuffer[mLength] = '\0';

    // One-shot shift: revert to ABC after typing one character
    if (mMode == KbModeShift)
        mMode = KbModeAbc;
}

// ==========================================================================
// ActivateKey  –  act on current cursor position
// ==========================================================================
void OnScreenKeyboard::ActivateKey()
{
    if (mCol == -1)
    {
        // Mode column
        switch (mRow)
        {
        case 0: Close(true);                              break;  // Done
        case 1: mMode = (mMode == KbModeShift) ? KbModeAbc : KbModeShift; break;
        case 2: mMode = (mMode == KbModeCaps)  ? KbModeAbc : KbModeCaps;  break;
        case 3: mMode = (mMode == KbModeSym)   ? KbModeAbc : KbModeSym;   break;
        }
        return;
    }

    // Character grid
    const char (*grid)[COL_COUNT] = (mMode == KbModeSym) ? KB_SYM : KB_ABC;
    char c = grid[mRow][mCol];
    if (c == '\0') return;

    // Apply case
    if (mMode != KbModeSym && c >= 'a' && c <= 'z')
    {
        if (mMode == KbModeShift || mMode == KbModeCaps)
            c = c - 32;  // uppercase
    }

    TypeChar(c);
}

// ==========================================================================
// MoveCursor
// ==========================================================================
void OnScreenKeyboard::MoveCursor(int dx, int dy)
{
    int newRow = mRow + dy;
    if (newRow < 0)          newRow = ROW_COUNT - 1;
    if (newRow >= ROW_COUNT) newRow = 0;

    int newCol = mCol;

    if (dy != 0)
    {
        // Keep column index — mode col stays mode col
        newCol = mCol;
    }
    else
    {
        newCol = mCol + dx;

        // Wrap between mode column (-1) and char grid (0..COL_COUNT-1)
        if (newCol < -1)          newCol = COL_COUNT - 1;
        if (newCol >= COL_COUNT)  newCol = -1;
    }

    mRow = newRow;
    mCol = newCol;
}

// ==========================================================================
// Update
// ==========================================================================
void OnScreenKeyboard::Update()
{
    if (!mOpen) return;

    if (InputManager::ControllerPressed(ControllerA,     -1)) { ActivateKey();  return; }
    if (InputManager::ControllerPressed(ControllerB,     -1)) { Close(false);   return; }
    if (InputManager::ControllerPressed(ControllerStart, -1)) { Close(true);    return; }

    // X = backspace (delete char before caret)
    if (InputManager::ControllerPressed(ControllerX, -1))
    {
        if (mCaretPos > 0)
        {
            // Shift everything after caret one position left
            for (int32_t i = mCaretPos - 1; i < mLength - 1; i++)
                mBuffer[i] = mBuffer[i + 1];
            mLength--;
            mCaretPos--;
            mBuffer[mLength] = '\0';
        }
        return;
    }

    // LT / RT = move caret left / right
    if (InputManager::ControllerPressed(ControllerLTrigger, -1))
    {
        if (mCaretPos > 0) mCaretPos--;
        return;
    }
    if (InputManager::ControllerPressed(ControllerRTrigger, -1))
    {
        if (mCaretPos < mLength) mCaretPos++;
        return;
    }

    // Left stick click = toggle Caps,  Right stick click = toggle Symbols
    if (InputManager::ControllerPressed(ControllerLThumb, -1))
    {
        mMode = (mMode == KbModeCaps) ? KbModeAbc : KbModeCaps;
        return;
    }
    if (InputManager::ControllerPressed(ControllerRThumb, -1))
    {
        mMode = (mMode == KbModeSym) ? KbModeAbc : KbModeSym;
        return;
    }

    // Y = space
    if (InputManager::ControllerPressed(ControllerY, -1))
    {
        TypeChar(' ');
        return;
    }

    if (InputManager::ControllerPressed(ControllerDpadUp,    -1)) { MoveCursor( 0,-1); return; }
    if (InputManager::ControllerPressed(ControllerDpadDown,  -1)) { MoveCursor( 0, 1); return; }
    if (InputManager::ControllerPressed(ControllerDpadLeft,  -1)) { MoveCursor(-1, 0); return; }
    if (InputManager::ControllerPressed(ControllerDpadRight, -1)) { MoveCursor( 1, 0); return; }
}

// ==========================================================================
// Render helpers
// ==========================================================================
static void DrawKey(float x, float y, float w, float h,
                    uint32_t bg, const char* label, uint32_t textCol,
                    bool selected, uint32_t accentColor)
{
    uint32_t drawBg = selected ? accentColor : bg;
    Drawing::DrawFilledRect(drawBg, x, y, w, h);

    // Subtle border on unselected keys
    if (!selected)
        Drawing::DrawFilledRect(0x18ffffff, x, y, w, 1.0f);

    float lw = 0.0f;
    Font::MeasureText(FONT_NORMAL, label, &lw);
    uint32_t col = selected ? 0xFFFFFFFF : textCol;
    Font::DrawText(FONT_NORMAL, label, col,
        (x + (w - lw) * 0.5f),
        (y + (h - 16.0f) * 0.5f));
}

// ==========================================================================
// Render
// ==========================================================================
void OnScreenKeyboard::Render()
{
    // MSVC 7.1: all locals declared at top of function
    float screenW, screenH, px, py;
    float inputX, inputY, inputW;
    float gridTop, modeX, charX, keyBlockW;
    float ry, ky, cx;
    float cw, tw, leftW, tx, caretX, visibleW;
    float stripY;
    float bsLblW, spLblW, iW, iGap, pairGap;
    float bsTotalW, spTotalW, pairW, gridRight, gridCentre, pairStart;
    float hbx, spx;
    float hintY, hx;
    float sel0w, sel1w, sel2w, ltrtW, totalHintW, itemGap;
    float lw, mstrW, stickIconW, stickGap, modeContentW, modeStartX;
    int row, col;
    bool modeActive, modeSel, keySel;
    uint32_t modeBg, modeTextCol, drawBg, tc, textCol, keyBg;
    char display_c, buf[3];
    const char* modeStr;
    const char* stickName;
    const char (*grid)[COL_COUNT];
    const char* modeLabels[ROW_COUNT];
    D3DTexture* stickIcon;
    D3DTexture* iconXbs;
    D3DTexture* iconYsp;
    D3DTexture* fi;
    std::string left, right, display, counter;

    if (!mOpen) return;

    screenW = (float)Context::GetScreenWidth();
    screenH = (float)Context::GetScreenHeight();

    // Dim overlay
    Drawing::DrawFilledRect(0xBB000000, 0.0f, 0.0f, screenW, screenH);

    // Panel
    px = (screenW - PANEL_W) * 0.5f;
    py = (screenH - PANEL_H) * 0.5f;
    Drawing::DrawFilledRect(0xFF181818, px, py, PANEL_W, PANEL_H);
    Drawing::DrawFilledRect(mAccentColor, px, py, PANEL_W, 3.0f);

    // Title (left) + counter (right)
    Font::DrawText(FONT_NORMAL, mTitle.c_str(), COLOR_WHITE,
        (px + MARGIN), (py + 10.0f));

    counter = String::Format("%d / %d", mLength, mMaxLength);
    cw = 0.0f;
    Font::MeasureText(FONT_NORMAL, counter, &cw);
    Font::DrawText(FONT_NORMAL, counter.c_str(), COLOR_TEXT_GRAY,
        (px + PANEL_W - MARGIN - cw), (py + 10.0f));

    // Input box
    inputX = px + MARGIN;
    inputY = py + 32.0f;
    inputW = PANEL_W - MARGIN * 2.0f;
    Drawing::DrawFilledRect(0xFF252525, inputX, inputY, inputW, INPUT_H);
    Drawing::DrawFilledRect(mAccentColor, inputX, inputY + INPUT_H - 2.0f, inputW, 2.0f);

    // Text with caret marker
    left    = std::string(mBuffer, mCaretPos);
    right   = std::string(mBuffer + mCaretPos);
    display = left + "|" + right;

    tw    = 0.0f;
    leftW = 0.0f;
    Font::MeasureText(FONT_NORMAL, display.c_str(), &tw);
    Font::MeasureText(FONT_NORMAL, (left + "|").c_str(), &leftW);

    tx       = inputX + 8.0f;
    caretX   = tx + leftW;
    visibleW = inputW - 12.0f;
    if (caretX > inputX + visibleW)
        tx = inputX + visibleW - leftW;
    if (tx > inputX + 8.0f)
        tx = inputX + 8.0f;

    Font::DrawText(FONT_NORMAL, display.c_str(), COLOR_WHITE,
        tx, (inputY + 10.0f));

    // ---- Key grid ----
    gridTop    = inputY + INPUT_H + KEY_GAP + 4.0f;
    keyBlockW  = MODE_COL_W + KEY_GAP + (float)COL_COUNT * (KEY_W + KEY_GAP) - KEY_GAP;
    modeX      = px + (PANEL_W - keyBlockW) * 0.5f;
    charX      = modeX + MODE_COL_W + KEY_GAP;

    grid = (mMode == KbModeSym) ? KB_SYM : KB_ABC;

    modeLabels[0] = "Done";
    modeLabels[1] = "Shift";
    modeLabels[2] = "Caps";
    modeLabels[3] = "#+=";

    for (row = 0; row < ROW_COUNT; row++)
    {
        ry = gridTop + (float)row * (KEY_H + KEY_GAP);

        // Mode key
        modeActive = false;
        if (row == 1) modeActive = (mMode == KbModeShift);
        if (row == 2) modeActive = (mMode == KbModeCaps);
        if (row == 3) modeActive = (mMode == KbModeSym);

        modeSel     = (mCol == -1 && mRow == row);
        modeBg      = modeActive ? 0xFF3A2A3A : 0xFF2A2A2A;
        if (row == 0) modeBg = 0xFF2A3A2A;
        modeTextCol = modeActive ? mAccentColor : COLOR_TEXT_GRAY;
        if (row == 0) modeTextCol = 0xFF88CC88;

        ky     = ry + (KEY_H - 16.0f) * 0.5f;
        drawBg = modeSel ? mAccentColor : modeBg;
        Drawing::DrawFilledRect(drawBg, modeX, ry, MODE_COL_W, KEY_H);
        if (!modeSel)
            Drawing::DrawFilledRect(0x18ffffff, modeX, ry, MODE_COL_W, 1.0f);

        tc = modeSel ? 0xFFFFFFFF : modeTextCol;

        if (row == 2 || row == 3)
        {
            modeStr   = (row == 2) ? "Caps" : "#+=";
            stickName = (row == 2) ? "StickLeft" : "StickRight";

            mstrW      = 0.0f;
            Font::MeasureText(FONT_NORMAL, modeStr, &mstrW);
            stickIcon  = TextureHelper::GetControllerIcon(stickName);
            stickIconW = stickIcon ? (float)ASSET_CONTROLLER_ICON_WIDTH : 0.0f;
            stickGap   = stickIcon ? 3.0f : 0.0f;
            modeContentW = mstrW + stickGap + stickIconW;
            modeStartX   = modeX + (MODE_COL_W - modeContentW) * 0.5f;

            Font::DrawText(FONT_NORMAL, modeStr, tc, modeStartX, ky);
            if (stickIcon)
                Drawing::DrawTexturedRect(stickIcon, 0xffffffff,
                    modeStartX + mstrW + stickGap,
                    ry + (KEY_H - (float)ASSET_CONTROLLER_ICON_HEIGHT) * 0.5f,
                    ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        }
        else
        {
            lw = 0.0f;
            Font::MeasureText(FONT_NORMAL, modeLabels[row], &lw);
            Font::DrawText(FONT_NORMAL, modeLabels[row], tc,
                (modeX + (MODE_COL_W - lw) * 0.5f), ky);
        }

        // Character keys
        for (col = 0; col < COL_COUNT; col++)
        {
            cx = charX + (float)col * (KEY_W + KEY_GAP);
            char c = grid[row][col];
            if (c == '\0') continue;

            display_c = c;
            if (mMode != KbModeSym && c >= 'a' && c <= 'z')
            {
                if (mMode == KbModeShift || mMode == KbModeCaps)
                    display_c = c - 32;
            }

            buf[0] = display_c;
            buf[1] = '\0';
            buf[2] = '\0';

            keySel  = (mCol == col && mRow == row);
            textCol = IsCharAllowed(c) ? COLOR_WHITE : 0xFF555555;
            keyBg   = (row == 0) ? 0xFF242424 : 0xFF2C2C2C;

            DrawKey(cx, ry, KEY_W, KEY_H, keyBg, buf, textCol, keySel, mAccentColor);
        }
    }

    // ---- Backspace + Space centred under the char grid ----
    stripY  = gridTop + (float)ROW_COUNT * (KEY_H + KEY_GAP) + 6.0f;
    bsLblW  = 0.0f;
    spLblW  = 0.0f;
    Font::MeasureText(FONT_NORMAL, "Backspace", &bsLblW);
    Font::MeasureText(FONT_NORMAL, "Space",     &spLblW);

    iW         = (float)ASSET_CONTROLLER_ICON_WIDTH;
    iGap       = 5.0f;
    pairGap    = 40.0f;
    bsTotalW   = iW + iGap + bsLblW;
    spTotalW   = iW + iGap + spLblW;
    pairW      = bsTotalW + pairGap + spTotalW;
    gridRight  = charX + (float)COL_COUNT * (KEY_W + KEY_GAP) - KEY_GAP;
    gridCentre = (charX + gridRight) * 0.5f;
    pairStart  = gridCentre - pairW * 0.5f;

    hbx      = pairStart;
    iconXbs  = TextureHelper::GetControllerIcon("ButtonX");
    if (iconXbs)
    {
        Drawing::DrawTexturedRect(iconXbs, 0xffffffff, hbx, stripY,
            ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        hbx += iW + iGap;
    }
    Font::DrawText(FONT_NORMAL, "Backspace", COLOR_TEXT_GRAY, hbx, (stripY + 2.0f));

    spx     = pairStart + bsTotalW + pairGap;
    iconYsp = TextureHelper::GetControllerIcon("ButtonY");
    if (iconYsp)
    {
        Drawing::DrawTexturedRect(iconYsp, 0xffffffff, spx, stripY,
            ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        spx += iW + iGap;
    }
    Font::DrawText(FONT_NORMAL, "Space", COLOR_TEXT_GRAY, spx, (stripY + 2.0f));

    // ---- Footer hints — measured and centred ----
    hintY = py + PANEL_H - 32.0f;

    // Pre-measure all items to compute total width
    sel0w   = 0.0f; Font::MeasureText(FONT_NORMAL, "Select",     &sel0w);
    sel1w   = 0.0f; Font::MeasureText(FONT_NORMAL, "Cancel",     &sel1w);
    sel2w   = 0.0f; Font::MeasureText(FONT_NORMAL, "Done",       &sel2w);
    ltrtW   = 0.0f; Font::MeasureText(FONT_NORMAL, "Move Cursor", &ltrtW);
    iW      = (float)ASSET_CONTROLLER_ICON_WIDTH;
    iGap    = 3.0f;
    itemGap = 18.0f;

    // A Select | B Cancel | Start Done | [LT][RT] Move Cursor
    totalHintW = (iW + iGap + sel0w + itemGap)
               + (iW + iGap + sel1w + itemGap)
               + (iW + iGap + sel2w + itemGap)
               + (iW + 2.0f + iW + iGap + ltrtW);

    hx = px + (PANEL_W - totalHintW) * 0.5f;
    fi = NULL;

    // A Select
    fi = TextureHelper::GetControllerIcon("ButtonA");
    if (fi) { Drawing::DrawTexturedRect(fi, 0xffffffff, hx, hintY,
        ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        hx += iW + iGap; }
    Font::DrawText(FONT_NORMAL, "Select", COLOR_TEXT_GRAY, hx, hintY);
    hx += sel0w + itemGap;

    // B Cancel
    fi = TextureHelper::GetControllerIcon("ButtonB");
    if (fi) { Drawing::DrawTexturedRect(fi, 0xffffffff, hx, hintY,
        ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        hx += iW + iGap; }
    Font::DrawText(FONT_NORMAL, "Cancel", COLOR_TEXT_GRAY, hx, hintY);
    hx += sel1w + itemGap;

    // Start Done
    fi = TextureHelper::GetControllerIcon("Start");
    if (fi) { Drawing::DrawTexturedRect(fi, 0xffffffff, hx, hintY,
        ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        hx += iW + iGap; }
    Font::DrawText(FONT_NORMAL, "Done", COLOR_TEXT_GRAY, hx, hintY);
    hx += sel2w + itemGap;

    // [LT][RT] Move Cursor
    fi = TextureHelper::GetControllerIcon("TriggerLeft");
    if (fi) { Drawing::DrawTexturedRect(fi, 0xffffffff, hx, hintY,
        ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        hx += iW + 2.0f; }
    fi = TextureHelper::GetControllerIcon("TriggerRight");
    if (fi) { Drawing::DrawTexturedRect(fi, 0xffffffff, hx, hintY,
        ASSET_CONTROLLER_ICON_WIDTH, ASSET_CONTROLLER_ICON_HEIGHT);
        hx += iW + iGap; }
    Font::DrawText(FONT_NORMAL, "Move Cursor", COLOR_TEXT_GRAY, hx, hintY);
}