#include "Main.h"
#include "Network.h"
#include "WebManager.h"
#include "TextureHelper.h"
#include "InputManager.h"
#include "StoreManager.h"
#include "Drawing.h"
#include "Font.h"
#include "Debug.h"
#include "String.h"
#include "Context.h"
#include "Scenes/SceneManager.h"
#include "Scenes/StoreScene.h"

static void CoverDownloadProgress(uint32_t dlNow, uint32_t dlTotal, void* userData)
{
    (void)userData;
    Debug::Print(std::string("Cover download: %u / %u bytes\n"), dlNow, dlTotal);
    // Example: to cancel the download, pass the same volatile bool* as progressUserData and pCancelRequested, then set:
    // *(volatile bool*)userData = true;
}

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D8             g_pD3D          = NULL;
LPDIRECT3DDEVICE8       g_pd3dDevice    = NULL;
LPDIRECT3D8             g_pD3D          = nullptr;
LPDIRECT3DDEVICE8       g_pd3dDevice    = nullptr;
SceneManager*           g_pSceneManager = nullptr;


typedef struct {
    DWORD dwWidth;
    DWORD dwHeight;
    BOOL fProgressive;
    BOOL fWideScreen;
    DWORD dwFreq;
} DISPLAY_MODE;

DISPLAY_MODE displayModes[] = {
    //{   720,    480,    TRUE,   TRUE,  60 },         // 720x480 progressive 16x9
    //{   720,    480,    TRUE,   FALSE, 60 },         // 720x480 progressive 4x3
    //{   720,    480,    FALSE,  TRUE,  50 },         // 720x480 interlaced 16x9 50Hz
    //{   720,    480,    FALSE,  FALSE, 50 },         // 720x480 interlaced 4x3  50Hz
    //{   720,    480,    FALSE,  TRUE,  60 },         // 720x480 interlaced 16x9
    //{   720,    480,    FALSE,  FALSE, 60 },         // 720x480 interlaced 4x3

    // Width  Height Progressive Widescreen

    // HDTV Progressive Modes
    //{1280, 720, TRUE, TRUE, 60}, // 1280x720 progressive 16x9

    // EDTV Progressive Modes
    {720, 480, TRUE, TRUE, 60}, // 720x480 progressive 16x9
    {640, 480, TRUE, TRUE, 60}, // 640x480 progressive 16x9
    {720, 480, TRUE, FALSE, 60}, // 720x480 progressive 4x3
    {640, 480, TRUE, FALSE, 60}, // 640x480 progressive 4x3

    // HDTV Interlaced Modes
    //    {  1920,   1080,    FALSE,  TRUE,  60 },         // 1920x1080 interlaced 16x9

    // SDTV PAL-50 Interlaced Modes
    {720, 480, FALSE, TRUE, 50}, // 720x480 interlaced 16x9 50Hz
    {640, 480, FALSE, TRUE, 50}, // 640x480 interlaced 16x9 50Hz
    {720, 480, FALSE, FALSE, 50}, // 720x480 interlaced 4x3  50Hz
    {640, 480, FALSE, FALSE, 50}, // 640x480 interlaced 4x3  50Hz

    // SDTV NTSC / PAL-60 Interlaced Modes
    {720, 480, FALSE, TRUE, 60}, // 720x480 interlaced 16x9
    {640, 480, FALSE, TRUE, 60}, // 640x480 interlaced 16x9
    {720, 480, FALSE, FALSE, 60}, // 720x480 interlaced 4x3
    {640, 480, FALSE, FALSE, 60}, // 640x480 interlaced 4x3
};

#define NUM_MODES (sizeof(displayModes) / sizeof(displayModes[0]))

bool SupportsMode(DISPLAY_MODE mode, DWORD dwVideoStandard, DWORD dwVideoFlags) {
    if (mode.dwFreq == 60 && !(dwVideoFlags & XC_VIDEO_FLAGS_PAL_60Hz) &&
        (dwVideoStandard == XC_VIDEO_STANDARD_PAL_I)) {
        return false;
    }
    if (mode.dwFreq == 50 && (dwVideoStandard != XC_VIDEO_STANDARD_PAL_I)) {
        return false;
    }
    if (mode.dwHeight == 480 && mode.fWideScreen && !(dwVideoFlags & XC_VIDEO_FLAGS_WIDESCREEN)) {
        return false;
    }
    if (mode.dwHeight == 480 && mode.fProgressive && !(dwVideoFlags & XC_VIDEO_FLAGS_HDTV_480p)) {
        return false;
    }
    if (mode.dwHeight == 720 && !(dwVideoFlags & XC_VIDEO_FLAGS_HDTV_720p)) {
        return false;
    }
    if (mode.dwHeight == 1080 && !(dwVideoFlags & XC_VIDEO_FLAGS_HDTV_1080i)) {
        return false;
    }
    return true;
}



//-----------------------------------------------------------------------------
// DeleteImageCache() - Remove all files in T:\Cache\Covers and T:\Cache\Screenshots
// Uncomment the call in main() to clear cache on startup (e.g. for testing).
//-----------------------------------------------------------------------------
static void DeleteImageCache()
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    std::string pattern;
    pattern = "T:\\Cache\\Covers\\*";
    h = FindFirstFileA( pattern.c_str(), &fd );
    if( h != INVALID_HANDLE_VALUE )
    {
        do
        {
            if( !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
            {
                std::string path = "T:\\Cache\\Covers\\" + std::string( fd.cFileName );
                DeleteFileA( path.c_str() );
            }
        } while( FindNextFileA( h, &fd ) );
        FindClose( h );
    }
    pattern = "T:\\Cache\\Screenshots\\*";
    h = FindFirstFileA( pattern.c_str(), &fd );
    if( h != INVALID_HANDLE_VALUE )
    {
        do
        {
            if( !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
            {
                std::string path = "T:\\Cache\\Screenshots\\" + std::string( fd.cFileName );
                DeleteFileA( path.c_str() );
            }
        } while( FindNextFileA( h, &fd ) );
        FindClose( h );
    }
}

bool InitD3D()
{
    uint32_t videoFlags = XGetVideoFlags();
    uint32_t videoStandard = XGetVideoStandard();
    uint32_t currentMode;
    for (currentMode = 0; currentMode < NUM_MODES - 1; currentMode++) {
        if (SupportsMode(displayModes[currentMode], videoStandard, videoFlags)) {
            break;
        }
    }

    LPDIRECT3D8 d3d = Direct3DCreate8(D3D_SDK_VERSION);
    if (d3d == nullptr) {
        Debug::Print("Failed to create d3d\n");
        return false;
    }

    D3DPRESENT_PARAMETERS params;
    ZeroMemory(&params, sizeof(params));
    params.BackBufferWidth = displayModes[currentMode].dwWidth;
    params.BackBufferHeight = displayModes[currentMode].dwHeight;
    params.Flags = displayModes[currentMode].fProgressive ? D3DPRESENTFLAG_PROGRESSIVE : D3DPRESENTFLAG_INTERLACED;
    params.Flags |= displayModes[currentMode].fWideScreen ? D3DPRESENTFLAG_WIDESCREEN : 0;
    params.FullScreen_RefreshRateInHz = displayModes[currentMode].dwFreq;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 1;
    params.EnableAutoDepthStencil = TRUE;
    params.AutoDepthStencilFormat = D3DFMT_D24S8;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    LPDIRECT3DDEVICE8 d3dDevice;
    if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &d3dDevice))) {
        Debug::Print("Failed to create device\n");
        return false;
    }

    d3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    d3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    d3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    d3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    d3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    d3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    
    Context::SetScreenSize(displayModes[currentMode].dwWidth, displayModes[currentMode].dwHeight);
    Context::SetD3dDevice(d3dDevice);
    return true;
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render()
{
    // Clear the backbuffer to dark gray
    g_pd3dDevice->Clear( 0, nullptr, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 
                        D3DCOLOR_XRGB(33,33,33), 1.0f, 0 );

    // Begin the scene
    if( SUCCEEDED( g_pd3dDevice->BeginScene() ) )
    {
        if( g_pSceneManager && g_pSceneManager->HasScene() )
            g_pSceneManager->Render( );

        g_pd3dDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pd3dDevice->Present( nullptr, nullptr, nullptr, nullptr );
}

//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize Direct3D
    if (InitD3D() == false)
    {
        OutputDebugString( "Failed to initialize Direct3D!\n" );
        return;
    }

    if( !CreateDirectory( "T:\\Cache", nullptr ) && GetLastError() != ERROR_ALREADY_EXISTS )
    {
        OutputDebugString( "Could not create T:\\Cache\n" );
    }
    if( !CreateDirectory( "T:\\Cache\\Covers", nullptr ) && GetLastError() != ERROR_ALREADY_EXISTS )
    {
        OutputDebugString( "Could not create T:\\Cache\\Covers\n" );
    }
    if( !CreateDirectory( "T:\\Cache\\Screenshots", nullptr ) && GetLastError() != ERROR_ALREADY_EXISTS )
    {
        OutputDebugString( "Could not create T:\\Cache\\Screenshots\n" );
    }
    //DeleteImageCache();  // Uncomment to clear image cache on startup

    InputManager::Init();
    Network::Init();
    WebManager::Init();
    WebManager::TrySyncTime();

    TextureHelper::Init();
    Drawing::Init();
    Font::Init();

    StoreManager::Init();
    

    /*AppsResponse appsResp;
    if (WebManager::TryGetApps(appsResp, 1, 20))
    {
        Debug::Print(String::Format("Apps: %u items, page %u/%u", (unsigned)appsResp.items.size(), appsResp.page, appsResp.totalPages));
    }

    {
        const std::string coverPath = "D:\\Media\\next-dashboard-cover.png";
        if (WebManager::TryDownloadCover("next-dashboard-t0h4", 256, 256, coverPath, CoverDownloadProgress, nullptr, nullptr))
        {
            Debug::Print(std::string("Cover saved to %s\n"), coverPath.c_str());
        }
        else
        {
            Debug::Print(std::string("Cover download failed\n"));
        }
    }*/

 

    g_pSceneManager = new SceneManager();
    Context::SetSceneManager( g_pSceneManager );

    // Initialize the store
    //g_pStore = new Store();
    //if( FAILED( g_pStore->Initialize() ) )
    //{
    //    OutputDebugString( "Failed to initialize Store!\n" );
    //    return;
    //}

    g_pSceneManager->PushScene( new StoreScene( ) );

    OutputDebugString( "Xbox Homebrew Store initialized successfully!\n" );

    while( TRUE )
    {
        InputManager::PumpInput();
        g_pSceneManager->Update();
        Render();
    }
}