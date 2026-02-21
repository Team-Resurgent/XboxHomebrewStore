#pragma once

#include "..\Main.h"
#include "Scene.h"

class SceneManager
{
public:
    SceneManager();
    ~SceneManager();

    void PushScene( Scene* pScene );
    void PopScene();
    bool HasScene() const;

    void Render();
    void Update();

private:
    struct SceneNode
    {
        Scene* pScene;
        SceneNode* pNext;
    };
    SceneNode* m_pStack;
};
