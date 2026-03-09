#pragma once

#include "Scene.h"
#include "..\Main.h"

class LoadingScene : public Scene
{
public:
    enum UpdateStatus
    {
        UPDATE_IDLE,
        UPDATE_DOWNLOADING,
        UPDATE_UNPACKING,
        UPDATE_REBOOTING,
        UPDATE_FAILED
    };

    LoadingScene();
    virtual ~LoadingScene();
    virtual void Render();
    virtual void Update();

    // Called by update thread to report progress
    volatile uint32_t mUpdateDlNow;
    volatile uint32_t mUpdateDlTotal;
    volatile UpdateStatus mUpdateStatus;
    volatile bool mUpdateCancelRequested;

    std::string mUpdateTag;
    std::string mUpdateZipUrl;

private:
    void LaunchUpdate();
    void ProceedToStore();

    int         mProgress;
    bool        mUpdateAvailable;
    bool        mUpdatePromptShown;
    HANDLE      mUpdateThread;
};