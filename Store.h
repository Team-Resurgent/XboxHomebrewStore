//=============================================================================
// Store.h - Xbox Homebrew Store Interface
//=============================================================================

#ifndef STORE_H
#define STORE_H

#include <xtl.h>
#include <xgraphics.h>
#include "XBFont.h"
#include "XBInput.h"

// UI State
enum UIState
{
    UI_MAIN_GRID,
    UI_ITEM_DETAILS,
    UI_DOWNLOADING,
    UI_SETTINGS
};

// Version info structure
struct VersionInfo
{
    char szVersion[16];
    char szChangelog[512];
    char szReleaseDate[32];  // "2024-02-11" or "Feb 11, 2024"
    char szTitleID[16];      // Xbox title ID (e.g., "45410026")
    char szRegion[16];       // Region code (e.g., "USA", "PAL", "JPN", "GLO")
    DWORD dwSize;
    int nState;  // Download state for this specific version
};

#define MAX_VERSIONS 10

// Store item structure
struct StoreItem
{
    char szName[64];
    char szDescription[256];
    char szAuthor[64];
    char szGUID[32];        // Global ID for cover art/media (e.g., "1000020")
    int nCategoryIndex;     // Index into categories array
    BOOL bNew;              // TRUE = Show red NEW badge, auto-clears on view
    
    // Multi-version support
    VersionInfo aVersions[MAX_VERSIONS];
    int nVersionCount;
    int nSelectedVersion;   // Which version is currently selected/highlighted
    int nVersionScrollOffset; // For scrolling version list
    BOOL bViewingVersionDetail; // TRUE when viewing a specific version's detail
    
    LPDIRECT3DTEXTURE8 pIcon;
};

// Dynamic category system
#define MAX_CATEGORIES 20

struct CategoryInfo
{
    char szName[32];        // "Games", "Emulators", etc.
    char szID[32];          // "games", "emulators", etc.
    int nAppCount;          // Number of apps in this category
};

class Store
{
public:
    Store();
    ~Store();

    HRESULT Initialize( LPDIRECT3DDEVICE8 pd3dDevice );
    void Update();
    void Render( LPDIRECT3DDEVICE8 pd3dDevice );

private:
    // Rendering functions
    void RenderMainGrid( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderItemDetails( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderDownloading( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderSettings( LPDIRECT3DDEVICE8 pd3dDevice );
    void RenderCategorySidebar( LPDIRECT3DDEVICE8 pd3dDevice );
    
    // Helper functions
    void DrawRect( LPDIRECT3DDEVICE8 pd3dDevice, float x, float y, float w, float h, DWORD color );
    void DrawText( LPDIRECT3DDEVICE8 pd3dDevice, const char* text, float x, float y, DWORD color );
    void DrawAppCard( LPDIRECT3DDEVICE8 pd3dDevice, StoreItem* pItem, float x, float y, BOOL bSelected );
    
    // Input handling
    void HandleInput();
    
    // Resolution and layout
    void DetectResolution( LPDIRECT3DDEVICE8 pd3dDevice );
    void CalculateLayout();
    
    // JSON parsing helpers
    BOOL LoadCatalogFromFile( const char* filename );
    const char* FindJSONValue( const char* json, const char* key );
    int GetOrCreateCategory( const char* catID );
    void BuildCategoryList();

    // Data
    StoreItem* m_pItems;
    int m_nItemCount;
    int m_nSelectedItem;
    int m_aFilteredIndices[100]; // Maps grid position to actual item index
    int m_nFilteredCount;
    UIState m_CurrentState;
    
    // Dynamic categories
    CategoryInfo m_aCategories[MAX_CATEGORIES];
    int m_nCategoryCount;
    int m_nSelectedCategory;
    
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
    CXBFont m_Font;
    
    // Controller state
    XBGAMEPAD* m_pGamepads;
    DWORD m_dwLastButtons;
};

// Vertex format for 2D rendering
struct CUSTOMVERTEX
{
    FLOAT x, y, z, rhw;
    DWORD color;
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

#endif // STORE_H