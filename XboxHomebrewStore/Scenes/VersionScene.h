#pragma once

#include "..\Main.h"
#include "Scene.h"
#include <string>
#include <vector>

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
    explicit VersionScene( const SelectedAppInfo& info );
    virtual void Render( LPDIRECT3DDEVICE8 pd3dDevice );
    virtual void Update();

private:
    void RenderListView( LPDIRECT3DDEVICE8 pd3dDevice );

    SelectedAppInfo m_info;
    int             m_selectedIndex;
};