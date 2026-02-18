#include "TextureHelper.h"
#include "Defines.h"
#include "String.h"
#include <d3dx8.h>

namespace {
    D3DDevice* mD3dDevice = NULL;
    D3DTexture* mBackground = NULL;
    D3DTexture* mScreenshot = NULL;
    D3DTexture* mCover = NULL;
}

bool TextureHelper::Init(D3DDevice* d3dDevice)
{
    mD3dDevice = d3dDevice;

    bool result = true;
    mBackground = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Background.jpg"));
    result &= mBackground == NULL;
    mScreenshot = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Screenshot.jpg"));
    result &= mScreenshot == NULL;
    mCover = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Cover.jpg"));
    result &= mCover == NULL;
    return result;
}

D3DTexture* TextureHelper::LoadFromFile(const std::string filePath)
{
    D3DTexture* tex = NULL;
    if (FAILED(D3DXCreateTextureFromFileEx(mD3dDevice, filePath.c_str(),
        D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT,
        D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &tex))) {
        return NULL;
    }
    return tex;
}

D3DTexture* TextureHelper::CopyTexture(D3DTexture* source)
{
    LPDIRECT3DSURFACE8 pSrcSurf = NULL;
    if( FAILED(source->GetSurfaceLevel( 0, &pSrcSurf ) ) ) return NULL;
    D3DSURFACE_DESC desc;
    if( FAILED( pSrcSurf->GetDesc( &desc ) ) ) {
        pSrcSurf->Release();
        return NULL;
    }
    pSrcSurf->Release();
    pSrcSurf = NULL;

    LPDIRECT3DTEXTURE8 pDest = NULL;
    if( FAILED(mD3dDevice->CreateTexture( desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_DEFAULT, &pDest ) ) ) {
        return NULL;
    }
    LPDIRECT3DSURFACE8 pDstSurf = NULL;
    if( FAILED( pDest->GetSurfaceLevel( 0, &pDstSurf ) ) ) {
        pDest->Release();
        return NULL;
    }
    if( FAILED(source->GetSurfaceLevel( 0, &pSrcSurf ) ) ) {
        pDstSurf->Release();
        pDest->Release();
        return NULL;
    }
    RECT rect = { 0, 0, (LONG)desc.Width, (LONG)desc.Height };
    POINT point = { 0, 0 };
    HRESULT hr = mD3dDevice->CopyRects( pSrcSurf, &rect, 1, pDstSurf, &point );
    pSrcSurf->Release();
    pDstSurf->Release();
    if( FAILED( hr ) ) {
        pDest->Release();
        return NULL;
    }
    return pDest;
}

D3DTexture* TextureHelper::GetBackground()
{
    return mBackground;
}

D3DTexture* TextureHelper::GetScreenshot()
{
    return mScreenshot != NULL ? CopyTexture(mScreenshot) : NULL;
}

D3DTexture* TextureHelper::GetCover()
{
    return mCover != NULL ? CopyTexture(mCover) : NULL;
}
