#pragma once

#include "..\Main.h"

class Scene
{
public:
    virtual ~Scene() {}
    virtual void Render() = 0;
    virtual void Update() = 0;
};