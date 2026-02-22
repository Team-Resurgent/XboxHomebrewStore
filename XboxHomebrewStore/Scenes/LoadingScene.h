#pragma once

#include "Scene.h"
#include "..\Main.h"

class LoadingScene : public Scene
{
public:
    LoadingScene();
    virtual ~LoadingScene();
    virtual void Render();
    virtual void Update();

private:
    int mProgress;  // 0..6: number of init steps completed
};
