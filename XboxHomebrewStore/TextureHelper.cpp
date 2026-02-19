#include "TextureHelper.h"
#include "Defines.h"
#include "String.h"
#include <d3dx8.h>
#include <string.h>
#include <cctype>

namespace {
    D3DDevice* mD3dDevice = NULL;
    D3DTexture* mBackground = NULL;
    D3DTexture* mHeader = NULL;
    D3DTexture* mFooter = NULL;
    D3DTexture* mSidebar = NULL;
    D3DTexture* mStore = NULL;
    D3DTexture* mCategoryHighlight = NULL;
    std::map<std::string, D3DTexture*> mCategoryIcons;
    D3DTexture* mScreenshot = NULL;
    D3DTexture* mCover = NULL;
}

bool TextureHelper::Init(D3DDevice* d3dDevice)
{
    mD3dDevice = d3dDevice;

    bool result = true;
    mBackground = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Background.jpg"));
    result &= mBackground == NULL;
    mHeader = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Header.png"));
    result &= mHeader == NULL;
    mFooter = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Footer.png"));
    result &= mFooter == NULL;
    mSidebar = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Sidebar.png"));
    result &= mSidebar == NULL;
    mCategoryHighlight = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "CategoryHighlight.png"));
    result &= mCategoryHighlight == NULL;
    mStore = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Store.png"));
    result &= mStore == NULL;

    std::string categoriesPath = String::Format( "%sCategories\\", MEDIA_PATH );
    std::string pattern = String::Format( "%s*.png", categoriesPath.c_str() );
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA( pattern.c_str(), &fd );
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                const char* dot = strrchr( fd.cFileName, '.' );
                std::string name = dot ? std::string( fd.cFileName, dot - fd.cFileName ) : fd.cFileName;
                for (size_t i = 0; i < name.size(); i++)
                    name[i] = (char)tolower( (unsigned char)name[i] );
                std::string path = String::Format( "%s%s", categoriesPath.c_str(), fd.cFileName );
                D3DTexture* tex = LoadFromFile( path );
                if (tex != NULL)
                    mCategoryIcons[name] = tex;
            }
        } while (FindNextFileA( h, &fd ));
        FindClose( h );
    }

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

D3DTexture* TextureHelper::GetHeader()
{
    return mHeader;
}

D3DTexture* TextureHelper::GetFooter()
{
    return mFooter;
}

D3DTexture* TextureHelper::GetSidebar()
{
    return mSidebar;
}

D3DTexture* TextureHelper::GetStore()
{
    return mStore;
}

D3DTexture* TextureHelper::GetCategoryHighlight()
{
    return mCategoryHighlight;
}

D3DTexture* TextureHelper::GetCategoryIcon(const std::string& name)
{
    std::string key = name;
    for (size_t i = 0; i < key.size(); i++) {
        key[i] = (char)tolower( (unsigned char)key[i] );
    }
    std::map<std::string, D3DTexture*>::const_iterator it = mCategoryIcons.find( key );
    if (it != mCategoryIcons.end()) {
        return it->second;
    }
    it = mCategoryIcons.find( "other" );
    if (it != mCategoryIcons.end()) {
        return it->second;
    }
    return mCategoryIcons.empty() ? NULL : mCategoryIcons.begin()->second;
}

D3DTexture* TextureHelper::GetScreenshot()
{
    return mScreenshot != NULL ? CopyTexture(mScreenshot) : NULL;
}

D3DTexture* TextureHelper::GetCover()
{
    return mCover != NULL ? CopyTexture(mCover) : NULL;
}
