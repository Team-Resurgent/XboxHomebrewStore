#pragma once

#include "..\Main.h"
#include "Scene.h"
#include "VersionScene.h"

class VersionDetailsScene : public Scene
{
public:
    VersionDetailsScene( const SelectedAppInfo& info, int versionIndex );
    virtual void Render();
    virtual void Update();

private:
    SelectedAppInfo m_info;
    int             m_versionIndex;
};
