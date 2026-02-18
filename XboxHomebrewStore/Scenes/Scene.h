#pragma once

#include "..\Main.h"

class Scene
{
public:
    virtual ~Scene() {}
    virtual void Render( LPDIRECT3DDEVICE8 pd3dDevice ) = 0;
    virtual void Update() = 0;
};