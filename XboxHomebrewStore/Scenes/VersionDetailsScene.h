//=============================================================================
// VersionDetailsScene.h - Scene showing a single version's detail
//=============================================================================

#ifndef VERSIONDETAILSSCENE_H
#define VERSIONDETAILSSCENE_H

#include "Scene.h"
#include "Main.h"
#include "VersionScene.h"

class VersionDetailsScene : public Scene
{
public:
    VersionDetailsScene( const SelectedAppInfo& info, int versionIndex );
    virtual void Render( LPDIRECT3DDEVICE8 pd3dDevice );
    virtual void Update();

private:
    SelectedAppInfo m_info;
    int             m_versionIndex;
};

#endif // VERSIONDETAILSSCENE_H
