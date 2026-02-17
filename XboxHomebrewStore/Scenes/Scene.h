//=============================================================================
// Scene.h - Base class for a scene (render + update)
//=============================================================================

#ifndef SCENE_H
#define SCENE_H

#include "..\Main.h"

class Scene
{
public:
    virtual ~Scene() {}
    virtual void Render( LPDIRECT3DDEVICE8 pd3dDevice ) = 0;
    virtual void Update() = 0;
};

#endif // SCENE_H
