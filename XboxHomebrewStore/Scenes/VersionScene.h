#pragma once

#include "Scene.h"

#include "..\Main.h"
#include "..\StoreManager.h"

/** Per-version info (no Store types). */
struct VersionInfo
{
    std::string id;
    std::string version;
    uint32_t    size;
    std::string releaseDate;
    std::string changeLog;
    std::string titleId;
    std::string region;
    int         state;  // STATE_* for display
};

/** Selected app data passed when opening VersionScene. */
struct SelectedAppInfo
{
    std::string              appId;
    std::string              appName;
    std::string              author;
    std::string              description;
    std::vector<VersionInfo> versions;
    void*                    pScreenshot;  // D3DTexture* for display (not owned)
};

class VersionScene : public Scene
{
public:
    explicit VersionScene(const StoreVersions& storeVersions);
    virtual void Render();
    virtual void Update();

private:
    void RenderHeader();
    void RenderFooter();
    void RenderVersionSidebar();
    void RenderListView();

    StoreVersions mStoreVersions;

    bool mSideBarFocused;
    int32_t mHighlightedVersionIndex;
    int32_t mVersionIndex;
};