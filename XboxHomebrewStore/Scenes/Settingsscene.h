#pragma once

#include "Scene.h"
#include "..\Main.h"
#include "..\AppSettings.h"
#include "..\OnScreenKeyboard.h"

// ---------------------------------------------------------------------------
// SettingsScene
//
// Pushed onto the stack from StoreScene when user presses Start.
// Popped back when user presses B.  Changes are auto-saved on exit.
//
// Controls
//   D-Pad Up/Down    navigate rows
//   D-Pad Left/Right cycle option values (for enum settings)
//   A                open folder browser (for path settings)
//   B                save & go back
//   Start            save & go back
// ---------------------------------------------------------------------------
class InstallPathScene;

class SettingsScene : public Scene
{
public:
    SettingsScene();
    virtual ~SettingsScene();
    virtual void Render();
    virtual void Update();
    virtual void OnResume();

    // Used by static path-browser callback
    void SetDownloadPath(const std::string& path) { mDownloadPath = path; }

private:
    void RenderHeader();
    void RenderFooter();
    void DrawFooterControl(float& x, float footerY, const char* iconName, const char* label);
    void RenderRows();
    void RenderPathRow(float x, float y, float w, bool selected, const char* label, const std::string& value);
    void RenderCycleRow(float x, float y, float w, bool selected, const char* label, const char* value);

    void OpenPathBrowser();
    void CycleAfterInstall(int dir);
    void SaveAndPop();

    // 0 = Download Path
    // 1 = After Install Action
    int32_t mSelectedRow;

    // Working copies — only written to AppSettings on save
    std::string        mDownloadPath;
    AfterInstallAction mAfterInstallAction;

    static const int ROW_COUNT = 2;
};