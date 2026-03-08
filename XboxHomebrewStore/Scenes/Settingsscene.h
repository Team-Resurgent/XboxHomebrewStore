#pragma once

#include "Scene.h"
#include "..\Main.h"
#include "..\AppSettings.h"
#include "..\StoreList.h"

// ---------------------------------------------------------------------------
// SettingsScene
//
// Three rows:
//   0 - Active Store         (A = push StoreManagerScene)
//   1 - Download Path        (A = browse)
//   2 - Keep Downloaded Files (A = open popup picker)
//
// B / Start  =  save & go back
// ---------------------------------------------------------------------------
class InstallPathScene;
class StoreManagerScene;

class SettingsScene : public Scene
{
public:
    SettingsScene();
    virtual ~SettingsScene();
    virtual void Render();
    virtual void Update();
    virtual void OnResume();

    void SetDownloadPath(const std::string& path) { mDownloadPath = path; }

private:
    void OpenPathBrowser();
    void SaveAndPop();
    void RenderPicker();

    int32_t mSelectedRow;

    std::string        mDownloadPath;
    AfterInstallAction mAfterInstallAction;

    // Popup picker state
    bool    mPickerOpen;
    int32_t mPickerSel;  // 0=Delete 1=Keep 2=Ask

    static const int ROW_COUNT = 3;
};