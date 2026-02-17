//=============================================================================
// SceneManager.h - Stack of scenes (push/pop, render/update top)
//=============================================================================

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include "Main.h"
#include "Scene.h"

class SceneManager
{
public:
    SceneManager();
    ~SceneManager();

    void PushScene( Scene* pScene );
    void PopScene();
    bool HasScene() const;

    void Render( LPDIRECT3DDEVICE8 pd3dDevice );
    void Update();

private:
    struct SceneNode
    {
        Scene* pScene;
        SceneNode* pNext;
    };
    SceneNode* m_pStack;
};

#endif // SCENEMANAGER_H
