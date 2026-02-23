#include "LoadingScene.h"
#include "SceneManager.h"
#include "StoreScene.h"

#include "..\Main.h"
#include "..\Context.h"
#include "..\Defines.h"
#include "..\Drawing.h"
#include "..\Font.h"
#include "..\String.h"
#include "..\InputManager.h"
#include "..\Network.h"
#include "..\WebManager.h"
#include "..\TextureHelper.h"
#include "..\StoreManager.h"
#include "..\DriveMount.h"
#include "..\FtpServer.h"

LoadingScene::LoadingScene() : mProgress(0)
{
}

LoadingScene::~LoadingScene()
{
}

void LoadingScene::Update()
{
    if (mProgress >= 8)
    {
        if (FileSystem::DirectoryCreate("T:\\Cache") == false)
        {
            OutputDebugString("Could not create T:\\Cache\n");
        }
        if (FileSystem::DirectoryCreate("T:\\Cache\\Covers") == false)
        {
            OutputDebugString("Could not create T:\\Cache\\Covers\n");
        }
        if (FileSystem::DirectoryCreate("T:\\Cache\\Screenshots") == false)
        {
            OutputDebugString( "Could not create T:\\Cache\\Screenshots\n" );
        }

        if (FileSystem::DirectoryCreate("HDD0-E:\\Homebrew") == false)
        {
            OutputDebugString( "Could not create HDD0-E:\\Homebrew\n" );
        }
        if (FileSystem::DirectoryCreate("HDD0-E:\\Homebrew\\Downloads") == false)
        {
            OutputDebugString( "Could not create HDD0-E:\\Homebrew\\Downloads\n" );
        }

        //DeleteImageCache();  // Uncomment to clear image cache on startup

        SceneManager* sceneManager = Context::GetSceneManager();
        sceneManager->PopScene();
        sceneManager->PushScene(new StoreScene());
        return;
    }

    switch (mProgress)
    {
    case 0: DriveMount::Init();         break;
    case 1: Network::Init();            break;
    case 2: WebManager::Init();         break;
    case 3: WebManager::TrySyncTime();  break;
    case 4: TextureHelper::Init();     break;
    case 5: StoreManager::Init();       break;
    case 6: FtpServer::Init();         break;
    default: break;
    }
    mProgress++;
}

void LoadingScene::Render()
{
    float w = Context::GetScreenWidth();
    float h = Context::GetScreenHeight();
    Drawing::DrawFilledRect(COLOR_BG, 0, 0, w, h);

    int step = (mProgress < 7) ? (mProgress + 1) : 7;
    std::string progressText = String::Format("%d of 7", step);
    float centerX = w * 0.5f;
    float centerY = h * 0.5f;
    float loadingW = 0.0f, progressW = 0.0f;
    Font::MeasureText(FONT_LARGE, "Loading...", &loadingW);
    Font::MeasureText(FONT_LARGE, progressText, &progressW);
    Font::DrawText(FONT_LARGE, "Loading...", COLOR_WHITE, centerX - loadingW * 0.5f, centerY - 40.0f);
    Font::DrawText(FONT_LARGE, progressText, COLOR_WHITE, centerX - progressW * 0.5f, centerY);
}
