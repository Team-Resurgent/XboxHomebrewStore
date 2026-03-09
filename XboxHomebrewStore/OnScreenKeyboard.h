#pragma once

#include "Main.h"

// ---------------------------------------------------------------------------
// OnScreenKeyboard
//
// Overlay keyboard widget with 4 modes selectable from a fixed left column.
//
// Layout
// ------
//   [Done ] [1][2][3][4][5][6][7][8][9][0]
//   [Shift] [q][w][e][r][t][y][u][i][o][p]   <- letters or symbols
//   [Caps ] [a][s][d][f][g][h][j][k][l][-]
//   [#+=  ] [z][x][c][v][b][n][m][_][@][.]
//            [Backspace X]        [Space Y]
//
// Modes
//   ABC    lowercase letters (default)
//   SHIFT  one-shot uppercase — reverts to ABC after one character
//   CAPS   uppercase lock
//   SYM    symbol character set
//
// Controls
//   A          type selected key / activate mode key
//   X          backspace
//   Y          space
//   B          cancel
//   D-Pad      move cursor
// ---------------------------------------------------------------------------

typedef enum KbFilter
{
    KbFilterNone     = 0,
    KbFilterFilename = 1,   // block chars illegal in folder/file names
    KbFilterNumbers  = 2,   // digits, dot, minus only
} KbFilter;

typedef enum KbMode
{
    KbModeAbc   = 0,
    KbModeShift = 1,
    KbModeCaps  = 2,
    KbModeSym   = 3,
} KbMode;

class OnScreenKeyboard
{
public:
    OnScreenKeyboard();

    void Open(const std::string& title,
              const std::string& prefill    = "",
              int32_t            maxLength   = 64,
              KbFilter           filter      = KbFilterNone,
              uint32_t           accentColor = 0xFFDF4088);

    void Update();
    void Render();

    bool IsOpen()      const { return mOpen; }
    bool IsConfirmed() const { return mConfirmed; }
    bool IsCancelled() const { return mCancelled; }
    std::string GetResult() const { return std::string(mBuffer); }

    void Reset();

private:
    void MoveCursor(int dx, int dy);
    void ActivateKey();
    bool IsCharAllowed(char c) const;
    void Close(bool confirm);
    void TypeChar(char c);

    // Grid is COL_COUNT wide, ROW_COUNT tall (not counting mode column)
    static const int ROW_COUNT = 4;
    static const int COL_COUNT = 10;

    // Character grids  [mode][row][col]  '\0' = no key
    static const char KB_ABC[ROW_COUNT][COL_COUNT];   // lowercase
    static const char KB_SYM[ROW_COUNT][COL_COUNT];   // symbols

    // State
    bool     mOpen;
    bool     mConfirmed;
    bool     mCancelled;

    std::string mTitle;
    int32_t     mMaxLength;
    KbFilter    mFilter;
    uint32_t    mAccentColor;

    char    mBuffer[128];
    int32_t mLength;
    int32_t mCaretPos;  // insertion point, 0..mLength

    // Cursor: col -1 = mode column, col 0..COL_COUNT-1 = char grid
    int32_t mRow;   // 0..ROW_COUNT-1
    int32_t mCol;   // -1 (mode col) or 0..COL_COUNT-1
    KbMode  mMode;

    // Panel dimensions
    static const float PANEL_W;
    static const float PANEL_H;
    static const float KEY_W;
    static const float KEY_H;
    static const float KEY_GAP;
    static const float MARGIN;
    static const float INPUT_H;
    static const float MODE_COL_W;  // width of the left mode-key column
};