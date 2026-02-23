#include "TextureHelper.h"
#include "Context.h"
#include "Defines.h"
#include "String.h"
namespace {
    D3DTexture* mBackground = nullptr;
    D3DTexture* mHeader = nullptr;
    D3DTexture* mFooter = nullptr;
    D3DTexture* mSidebar = nullptr;
    D3DTexture* mStore = nullptr;
    D3DTexture* mCategoryHighlight = nullptr;
    D3DTexture* mCard = nullptr;
    D3DTexture* mCardHighlight = nullptr;
    D3DTexture* mNewBadge = nullptr;
    D3DTexture* mUpdateBadge = nullptr;
    std::map<std::string, D3DTexture*> mCategoryIcons;
    std::map<std::string, D3DTexture*> mControllerIcons;
    D3DTexture* mScreenshot = nullptr;
    D3DTexture* mCover = nullptr;
}

bool TextureHelper::Init()
{
    bool result = true;
    mBackground = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Background.jpg"));
    result &= mBackground == nullptr;
    mHeader = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Header.png"));
    result &= mHeader == nullptr;
    mFooter = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Footer.png"));
    result &= mFooter == nullptr;
    mSidebar = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Sidebar.png"));
    result &= mSidebar == nullptr;
    mCategoryHighlight = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "CategoryHighlight.png"));
    result &= mCategoryHighlight == nullptr;
    mCard = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Card.png"));
    result &= mCard == nullptr;
    mCardHighlight = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "CardHighlight.png"));
    result &= mCardHighlight == nullptr;
    mNewBadge = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "NewBadge.png"));
    result &= mNewBadge == nullptr;
    mUpdateBadge = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "UpdateBadge.png"));
    result &= mUpdateBadge == nullptr;
    mStore = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Store.png"));
    result &= mStore == nullptr;

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
                if (tex != nullptr) {
                    mCategoryIcons[name] = tex;
                }
            }
        } while (FindNextFileA( h, &fd ));
        FindClose( h );
    }

    std::string controllerPath = String::Format( "%sController\\", MEDIA_PATH );
    std::string controllerPattern = String::Format( "%s*.png", controllerPath.c_str() );
    HANDLE hController = FindFirstFileA( controllerPattern.c_str(), &fd );
    if (hController != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                const char* dot = strrchr( fd.cFileName, '.' );
                std::string name = dot ? std::string( fd.cFileName, dot - fd.cFileName ) : fd.cFileName;
                for (size_t i = 0; i < name.size(); i++)
                    name[i] = (char)tolower( (unsigned char)name[i] );
                std::string path = String::Format( "%s%s", controllerPath.c_str(), fd.cFileName );
                D3DTexture* tex = LoadFromFile( path );
                if (tex != nullptr) {
                    mControllerIcons[name] = tex;
                }
            }
        } while (FindNextFileA( hController, &fd ));
        FindClose( hController );
    }

    mScreenshot = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Screenshot.jpg"));
    result &= mScreenshot == nullptr;
    mCover = LoadFromFile(String::Format( "%s%s", MEDIA_PATH, "Cover.jpg"));
    result &= mCover == nullptr;
    return result;
}

D3DTexture* TextureHelper::LoadFromFile(const std::string filePath)
{
    D3DTexture* tex = nullptr;
    if (FAILED(D3DXCreateTextureFromFileEx(Context::GetD3dDevice(), filePath.c_str(),
        D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT,
        D3DX_DEFAULT, D3DX_DEFAULT, 0, nullptr, nullptr, &tex))) {
        return nullptr;
    }
    return tex;
}

D3DTexture* TextureHelper::CopyTexture(D3DTexture* source)
{
    LPDIRECT3DSURFACE8 pSrcSurf = nullptr;
    if( FAILED(source->GetSurfaceLevel( 0, &pSrcSurf ) ) ) return nullptr;
    D3DSURFACE_DESC desc;
    if( FAILED( pSrcSurf->GetDesc( &desc ) ) ) {
        pSrcSurf->Release();
        return nullptr;
    }
    pSrcSurf->Release();
    pSrcSurf = nullptr;

    D3DTexture* pDest = nullptr;
    if( FAILED(Context::GetD3dDevice()->CreateTexture( desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_DEFAULT, &pDest ) ) ) {
        return nullptr;
    }
    LPDIRECT3DSURFACE8 pDstSurf = nullptr;
    if( FAILED( pDest->GetSurfaceLevel( 0, &pDstSurf ) ) ) {
        pDest->Release();
        return nullptr;
    }
    if( FAILED(source->GetSurfaceLevel( 0, &pSrcSurf ) ) ) {
        pDstSurf->Release();
        pDest->Release();
        return nullptr;
    }
    RECT rect = { 0, 0, (LONG)desc.Width, (LONG)desc.Height };
    POINT point = { 0, 0 };
    HRESULT hr = Context::GetD3dDevice()->CopyRects( pSrcSurf, &rect, 1, pDstSurf, &point );
    pSrcSurf->Release();
    pDstSurf->Release();
    if( FAILED( hr ) ) {
        pDest->Release();
        return nullptr;
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

D3DTexture* TextureHelper::GetCard()
{
    return mCard;
}

D3DTexture* TextureHelper::GetCardHighlight()
{
    return mCardHighlight;
}

D3DTexture* TextureHelper::GetNewBadge()
{
    return mNewBadge;
}

D3DTexture* TextureHelper::GetUpdateBadge()
{
    return mUpdateBadge;
}

D3DTexture* TextureHelper::GetCategoryIcon(const std::string name)
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
    return mCategoryIcons.empty() ? nullptr : mCategoryIcons.begin()->second;
}

D3DTexture* TextureHelper::GetControllerIcon(const std::string name)
{
    std::string key = name;
    for (size_t i = 0; i < key.size(); i++)
        key[i] = (char)tolower( (unsigned char)key[i] );
    std::map<std::string, D3DTexture*>::const_iterator it = mControllerIcons.find( key );
    return (it != mControllerIcons.end()) ? it->second : nullptr;
}

D3DTexture* TextureHelper::GetScreenshotRef()
{
    return mScreenshot;
}

D3DTexture* TextureHelper::GetCoverRef()
{
    return mCover;
}