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
    char szChangelog[128];
    char szReleaseDate[32];  // "2024-02-11" or "Feb 11, 2024"
    char szTitleID[16];      // Xbox title ID (e.g., "45410026")
    char szRegion[16];       // Region code (e.g., "USA", "PAL", "JPN", "GLO")
    char szInstallPath[128]; // Install location (e.g., "E:\\Apps\\XBMC35")
    char szGUID[32];         // ADDED: Version-specific download ID (server-side only)
    DWORD dwSize;
    int nState;  // Download state for this specific version
};

#define MAX_VERSIONS 32

// Store item structure
struct StoreItem
{
    char szName[64];
    char szDescription[256];
    char szAuthor[64];
    char szGUID[32];        // Legacy: Global ID for cover art/media (backwards compat)
    char szID[64];          // ADDED: App-level unique ID (safe for user_state.json)
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
#define MAX_CATEGORIES 128

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
    
    // User state management
    BOOL LoadUserState( const char* filename );
    BOOL SaveUserState( const char* filename );
    void MergeUserStateWithCatalog();
    void MarkAppAsViewed( const char* appId );
    void SetVersionState( const char* appId, const char* version, int state );

    // UTF-8 and JSON helpers (for user_state and version changelogs)
    void SafeCopyUTF8( char* dest, const char* src, int maxBytes );
    void UnescapeJSON( char* dest, const char* src, int maxLen );
    
    // Update detection helpers
    BOOL HasUpdateAvailable( StoreItem* pItem );
    int GetDisplayState( StoreItem* pItem, int versionIndex );
    
    // Helper functions
    void DrawRect( LPDIRECT3DDEVICE8 pd3dDevice, float x, float y, float w, float h, DWORD color );
    void DrawText( LPDIRECT3DDEVICE8 pd3dDevice, const char* text, float x, float y, DWORD color );
    void DrawAppCard( LPDIRECT3DDEVICE8 pd3dDevice, StoreItem* pItem, float x, float y, BOOL bSelected );
    
    // Input handling
    void HandleInput();
    
    // Resolution and layout
    void DetectResolution( LPDIRECT3DDEVICE8 pd3dDevice );
    void CalculateLayout();
    
    // Catalog loading (web; fallback uses GetOrCreateCategory + BuildCategoryList)
    BOOL LoadCatalogFromWeb();
    BOOL LoadCategoriesFromWeb();
    BOOL LoadAppsPage( int page, const char* categoryFilter );
    void EnsureVersionsForItem( StoreItem* pItem );
    int FindCategoryIndex( const char* catID ) const;
    int GetOrCreateCategory( const char* catID );
    void BuildCategoryList();

    // Data
    StoreItem* m_pItems;
    int m_nItemCount;
    int m_nSelectedItem;
    int* m_pFilteredIndices; // Maps grid position to actual item index
    int m_nFilteredCount;
    UIState m_CurrentState;
    
    // Dynamic categories
    CategoryInfo m_aCategories[MAX_CATEGORIES];
    int m_nCategoryCount;
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
    CXBFont m_Font;
    
    // Controller state
    XBGAMEPAD* m_pGamepads;
    DWORD m_dwLastButtons;

    // SafeCopy
    void SafeCopy(char* dest, const char* src, int maxLen);
    void GenerateAppID(const char* name, char* outID, int maxLen);
};

// Vertex format for 2D rendering
struct CUSTOMVERTEX
{
    FLOAT x, y, z, rhw;
    DWORD color;
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

#endif // STORE_H