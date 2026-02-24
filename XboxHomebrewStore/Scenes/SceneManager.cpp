#include "SceneManager.h"

SceneManager::SceneManager() : m_pStack(nullptr)
{
}

SceneManager::~SceneManager()
{
    while (HasScene()) {
        PopScene();
    }
}

void SceneManager::PushScene(Scene* pScene)
{
    if (!pScene) {
        return;
    }
    SceneNode* pNode = new SceneNode;
    pNode->pScene = pScene;
    pNode->pNext = m_pStack;
    m_pStack = pNode;
}

void SceneManager::PopScene()
{
    if (m_pStack == nullptr) {
        return;
    }
    SceneNode* pNode = m_pStack;
    m_pStack = pNode->pNext;
    delete pNode->pScene;
    delete pNode;
    if (m_pStack && m_pStack->pScene) {
        m_pStack->pScene->OnResume();
    }
}

bool SceneManager::HasScene() const
{
    return m_pStack != nullptr;
}

void SceneManager::Render( )
{
    if (m_pStack && m_pStack->pScene) {
        m_pStack->pScene->Render();
    }
}

void SceneManager::Update()
{
    if (m_pStack && m_pStack->pScene) {
        m_pStack->pScene->Update();
    }
}
