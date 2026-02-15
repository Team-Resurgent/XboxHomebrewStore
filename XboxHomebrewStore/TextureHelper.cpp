//=============================================================================
// TextureHelper.cpp - Load and store media textures
//=============================================================================

#include "TextureHelper.h"
#include "String.h"
#include <d3dx8.h>

LPDIRECT3DTEXTURE8 TextureHelper::s_pScreenshot = NULL;
LPDIRECT3DTEXTURE8 TextureHelper::s_pCover = NULL;

static const char* MEDIA_PATH = "D:\\Media\\";

static HRESULT LoadTextureFromFile( LPDIRECT3DDEVICE8 pd3dDevice, const char* filename, LPDIRECT3DTEXTURE8* ppTexture )
{
    if( !pd3dDevice || !filename || !ppTexture ) return E_INVALIDARG;
    std::string path = ( filename[1] != ':' ) ? String::Format( "%s%s", MEDIA_PATH, filename ) : std::string( filename );
    return D3DXCreateTextureFromFileEx( pd3dDevice, path.c_str(),
        D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT,
        D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, ppTexture );
}

HRESULT TextureHelper::Init( LPDIRECT3DDEVICE8 pd3dDevice )
{
    if( !pd3dDevice ) return E_INVALIDARG;
    HRESULT hr;
    if( FAILED( hr = LoadTextureFromFile( pd3dDevice, "Screenshot.jpg", &s_pScreenshot ) ) ) {
        return hr;
    }
    if( FAILED( hr = LoadTextureFromFile( pd3dDevice, "Cover.jpg", &s_pCover ) ) ) {
        if( s_pScreenshot ) { s_pScreenshot->Release(); s_pScreenshot = NULL; }
        return hr;
    }
    return S_OK;
}

LPDIRECT3DTEXTURE8 TextureHelper::CopyTexture( LPDIRECT3DDEVICE8 pd3dDevice, LPDIRECT3DTEXTURE8 pSource )
{
    if( !pd3dDevice || !pSource ) return NULL;
    LPDIRECT3DSURFACE8 pSrcSurf = NULL;
    if( FAILED( pSource->GetSurfaceLevel( 0, &pSrcSurf ) ) ) return NULL;
    D3DSURFACE_DESC desc;
    if( FAILED( pSrcSurf->GetDesc( &desc ) ) ) {
        pSrcSurf->Release();
        return NULL;
    }
    pSrcSurf->Release();
    pSrcSurf = NULL;

    LPDIRECT3DTEXTURE8 pDest = NULL;
    if( FAILED( pd3dDevice->CreateTexture( desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_DEFAULT, &pDest ) ) ) {
        return NULL;
    }
    LPDIRECT3DSURFACE8 pDstSurf = NULL;
    if( FAILED( pDest->GetSurfaceLevel( 0, &pDstSurf ) ) ) {
        pDest->Release();
        return NULL;
    }
    if( FAILED( pSource->GetSurfaceLevel( 0, &pSrcSurf ) ) ) {
        pDstSurf->Release();
        pDest->Release();
        return NULL;
    }
    RECT rect = { 0, 0, (LONG)desc.Width, (LONG)desc.Height };
    POINT point = { 0, 0 };
    HRESULT hr = pd3dDevice->CopyRects( pSrcSurf, &rect, 1, pDstSurf, &point );
    pSrcSurf->Release();
    pDstSurf->Release();
    if( FAILED( hr ) ) {
        pDest->Release();
        return NULL;
    }
    return pDest;
}

LPDIRECT3DTEXTURE8 TextureHelper::GetScreenshot( LPDIRECT3DDEVICE8 pd3dDevice )
{
    return s_pScreenshot ? CopyTexture( pd3dDevice, s_pScreenshot ) : NULL;
}

LPDIRECT3DTEXTURE8 TextureHelper::GetCover( LPDIRECT3DDEVICE8 pd3dDevice )
{
    return s_pCover ? CopyTexture( pd3dDevice, s_pCover ) : NULL;
}
