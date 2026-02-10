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

// Store item structure
struct StoreItem
{
    char szName[64];
    char szDescription[256];
    char szVersion[16];
    char szAuthor[64];
    DWORD dwSize;           // Size in bytes
    int nCategoryIndex;     // Index into categories array
    
    // Download/install state
    enum State {
        STATE_NOT_DOWNLOADED = 0,
        STATE_DOWNLOADED,
        STATE_INSTALLED,
        STATE_UPDATE_AVAILABLE
    } eState;
    
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