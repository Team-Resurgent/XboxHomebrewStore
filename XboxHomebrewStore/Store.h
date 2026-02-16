//=============================================================================
// Store.h - Xbox Homebrew Store Interface
//=============================================================================

#ifndef STORE_H
#define STORE_H

#include <xtl.h>
#include <xgraphics.h>
#include "XBFont.h"
#include "XBInput.h"
#include "WebManager.h"
#include "Models.h"
#include "TextureHelper.h"
#include "ImageDownloader.h"

// UI State
enum UIState
{
    UI_MAIN_GRID,
    UI_ITEM_DETAILS,
    UI_DOWNLOADING,
    UI_SETTINGS
};

// Store item: app data (from API) + UI state
struct StoreItem
{
    AppItem app;
    int nCategoryIndex;     // Index into categories array
    std::vector<VersionItem> versions;
    int nSelectedVersion;
    int nVersionScrollOffset;
    BOOL bViewingVersionDetail;
    LPDIRECT3DTEXTURE8 pIcon;
    LPDIRECT3DTEXTURE8 pScreenshot;
};

#include "UserState.h"

class Store
{
public:
    Store();
    ~Store();

    HRESULT Initialize( LPDIRECT3DDEVICE8 pd3dDevice );
    void Update();
    void Render( LPDIRECT3DDEVICE8 pd3dDevice );

    void SetDownloadProgress( uint32_t dlNow, uint32_t dlTotal );

private:
    // Rendering functions
    void RenderMainGrid( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderItemDetails( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderDownloading( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderSettings( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderCategorySidebar( LPDIRECT3DDEVICE8 pd3dDevice );
    
    // User state (load/save and apply to store)
    void MarkAppAsViewed( const char* appId );
    void SetVersionState( const char* appId, const char* version, int state );

    // Update detection helpers
    BOOL HasUpdateAvailable( StoreItem* pItem );
    int GetDisplayState( StoreItem* pItem, int versionIndex );
    
    // Helper functions
    void DrawRect( LPDIRECT3DDEVICE8 pd3dDevice, float x, float y, float w, float h, DWORD color );
    void DrawTexturedRect( LPDIRECT3DDEVICE8 pd3dDevice, float x, float y, float w, float h, LPDIRECT3DTEXTURE8 pTexture );
    void DrawText( LPDIRECT3DDEVICE8 pd3dDevice, const char* text, float x, float y, DWORD color );
    void DrawAppCard( LPDIRECT3DDEVICE8 pd3dDevice, StoreItem* pItem, float x, float y, BOOL bSelected );
    
    // Input handling
    void HandleInput();
    
    // Resolution and layout
    void DetectResolution( LPDIRECT3DDEVICE8 pd3dDevice );
    void CalculateLayout();
    
    // Catalog loading (web)
    BOOL LoadCatalogFromWeb();
    BOOL LoadCategoriesFromWeb();
    BOOL LoadAppsPage( int page, const char* categoryFilter );
    void EnsureVersionsForItem( StoreItem* pItem );
    void EnsureScreenshotForItem( StoreItem* pItem );
    int FindCategoryIndex( const char* catID ) const;
    void BuildCategoryList();

    // Data
    UserState m_userState;
    ImageDownloader m_imageDownloader;
    LPDIRECT3DDEVICE8 m_pd3dDevice;
    StoreItem* m_pItems;
    int m_nItemCount;
    int m_nSelectedItem;
    int* m_pFilteredIndices; // Maps grid position to actual item index
    int m_nFilteredCount;
    UIState m_CurrentState;
    
    // Dynamic categories
    std::vector<CategoryItem> m_aCategories;
    int m_nSelectedCategory;

    // Pagination (web catalog)
    int m_nCurrentPage;
    int m_nTotalPages;
    int m_nTotalCount;
    
    // Screen dimensions (detected)
    float m_fScreenWidth;
    float m_fScreenHeight;
    
    // Dynamic layout values
    float m_fSidebarWidth;
    float m_fGridStartX;
    float m_fGridStartY;
    float m_fCardWidth;
    float m_fCardHeight;
    int m_nGridCols;
    int m_nGridRows;
    
    // D3D resources
    LPDIRECT3DVERTEXBUFFER8 m_pVB;
    LPDIRECT3DVERTEXBUFFER8 m_pVBTex;
    CXBFont m_Font;
    
    // Controller state
    XBGAMEPAD* m_pGamepads;
    DWORD m_dwLastButtons;

    // App download state (UI_DOWNLOADING)
    std::string m_downloadVersionId;
    std::string m_downloadAppName;
    std::string m_downloadPath;
    volatile uint32_t m_downloadNow;
    volatile uint32_t m_downloadTotal;
    volatile bool m_downloadDone;
    bool m_downloadSuccess;
    volatile bool m_downloadCancelRequested;
    HANDLE m_downloadThread;
    static DWORD WINAPI DownloadThreadProc( LPVOID param );
    void StartDownloadThread();

};

// Vertex format for 2D rendering
struct CUSTOMVERTEX
{
    FLOAT x, y, z, rhw;
    DWORD color;
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

// Vertex format for textured quads (icon/screenshot)
struct TEXVERTEX
{
    FLOAT x, y, z, rhw;
    DWORD color;
    FLOAT tu, tv;
};

#define D3DFVF_TEXVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

#endif // STORE_H