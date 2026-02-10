//=============================================================================
// Xbox Homebrew Store
// Main Application Entry Point
//=============================================================================

#include <xtl.h>
#include <xgraphics.h>
#include "Store.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D8             g_pD3D       = NULL;
LPDIRECT3DDEVICE8       g_pd3dDevice = NULL;
Store*                  g_pStore     = NULL;

//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
HRESULT InitD3D()
{
    // Create the D3D object
    if( NULL == ( g_pD3D = Direct3DCreate8( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice
    D3DPRESENT_PARAMETERS d3dpp;
ZeroMemory(&d3dpp, sizeof(d3dpp));

d3dpp.BackBufferWidth  = 1280;
d3dpp.BackBufferHeight = 720;

/*
IMPORTANT:
Use LINEAR backbuffer if you use XFONT text rendering
(Swizzled surfaces break font rendering)
*/
d3dpp.BackBufferFormat = D3DFMT_LIN_X8R8G8B8;

d3dpp.BackBufferCount  = 1;
d3dpp.EnableAutoDepthStencil = TRUE;
d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;

/*
Tell Xbox to use 720p HDTV mode
*/
d3dpp.Flags = D3DPRESENTFLAG_PROGRESSIVE | D3DPRESENTFLAG_WIDESCREEN;

d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;


    // Create the Direct3D device
    if( FAILED( g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice ) ) )
    {
        return E_FAIL;
    }

    // Device state setup
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
VOID Cleanup()
{
    if( g_pStore != NULL )
    {
        delete g_pStore;
        g_pStore = NULL;
    }

    if( g_pd3dDevice != NULL )
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }

    if( g_pD3D != NULL )
    {
        g_pD3D->Release();
        g_pD3D = NULL;
    }
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render()
{
    // Clear the backbuffer to dark gray
    g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 
                        D3DCOLOR_XRGB(33,33,33), 1.0f, 0 );

    // Begin the scene
    if( SUCCEEDED( g_pd3dDevice->BeginScene() ) )
    {
        // Render the store UI
        if( g_pStore )
            g_pStore->Render( g_pd3dDevice );

        // End the scene
        g_pd3dDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}

//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize Xbox input devices - MUST be called before XBInput functions
    XInitDevices( 0, NULL );

    // Initialize Direct3D
    if( FAILED( InitD3D() ) )
    {
        OutputDebugString( "Failed to initialize Direct3D!\n" );
        return;
    }

    // Initialize the store
    g_pStore = new Store();
    if( FAILED( g_pStore->Initialize( g_pd3dDevice ) ) )
    {
        OutputDebugString( "Failed to initialize Store!\n" );
        Cleanup();
        return;
    }

    OutputDebugString( "Xbox Homebrew Store initialized successfully!\n" );

    // Enter the message loop
    while( TRUE )
    {
        // Update store logic (handle input, etc)
        g_pStore->Update();

        // Render the scene
        Render();
    }

    // Clean up everything
    Cleanup();
}