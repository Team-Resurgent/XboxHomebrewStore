#pragma once

#include "Main.h"

// ---------------------------------------------------------------------------
// OnScreenKeyboard
//
// Self-contained overlay keyboard widget.  Drop one into any scene as a
// member, then each frame call Update() and Render() while IsOpen() is true.
//
// Usage
// -----
//   // Open with optional pre-fill, title and max length
//   mKeyboard.Open("My Title", "prefill text", 64);
//
//   // Each frame:
//   if (mKeyboard.IsOpen()) {
//       mKeyboard.Update();
//       // check result before Render so the overlay disappears this frame
//       if (mKeyboard.IsConfirmed()) {
//           std::string result = mKeyboard.GetResult();
//           // do something with result
//       }
//   }
//
//   // In your Render():
//   if (mKeyboard.IsOpen())
//       mKeyboard.Render();
//
// Controls (while open)
//   A        type selected key / press OK
//   X        backspace
//   Y        toggle shift
//   B        cancel (closes without confirming)
//   D-Pad    move cursor
//
// Character filter
//   SetFilter(KB_FILTER_FILENAME)  blocks  \ / : * ? " < > |
//   SetFilter(KB_FILTER_NUMBERS)   allows  0-9 . - only
//   SetFilter(KB_FILTER_NONE)      no restrictions (default)
// ---------------------------------------------------------------------------

typedef enum KbFilter
{
    KbFilterNone     = 0,
    KbFilterFilename = 1,   // block chars illegal in folder/file names
    KbFilterNumbers  = 2,   // digits, dot, minus only
} KbFilter;

class OnScreenKeyboard
{
public:
    OnScreenKeyboard();

    // Open the keyboard.  Call each time you want to show it.
    //   title     – label shown above the input box (e.g. "New Folder Name")
    //   prefill   – initial text in the input box (pass "" for blank)
    //   maxLength – hard character limit (default 64, max 127)
    //   filter    – optional character filter
    //   accentColor – ARGB colour used for the panel accent bar and selection
    void Open(const std::string& title,
              const std::string& prefill   = "",
              int32_t            maxLength  = 64,
              KbFilter           filter     = KbFilterNone,
              uint32_t           accentColor = 0xFFDF4088);

    // Call every frame while IsOpen()
    void Update();

    // Call from your scene's Render() while IsOpen()
    void Render();

    // State queries
    bool IsOpen()      const { return mOpen; }
    bool IsConfirmed() const { return mConfirmed; }
    bool IsCancelled() const { return mCancelled; }

    // Returns the typed text.  Valid after IsConfirmed() is true.
    std::string GetResult() const { return std::string(mBuffer); }

    // Reset state ready for next Open() call (called automatically by Open)
    void Reset();

private:
    void MoveCursor(int dx, int dy);
    void TypeKey();
    bool IsCharAllowed(char c) const;
    void Close(bool confirm);

    // Layout
    static const int KB_ROWS = 5;
    static const char* KB_ROW_LABELS[KB_ROWS][14];
    static const int   KB_ROW_LEN[KB_ROWS];

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

    int32_t mRow;
    int32_t mCol;
    bool    mShift;

    // Visual constants
    static const float PANEL_W;
    static const float PANEL_H;
    static const float KEY_W;
    static const float KEY_H;
    static const float KEY_GAP;
    static const float MARGIN;
    static const float INPUT_H;
};