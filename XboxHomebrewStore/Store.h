#pragma once

#include <xtl.h>
#include <xgraphics.h>
#include "Defines.h"
#include "WebManager.h"
#include "Models.h"
#include "TextureHelper.h"
#include "ImageDownloader.h"

enum UIState
{
    UI_MAIN_GRID,
    UI_ITEM_DETAILS,
    UI_DOWNLOADING,
    UI_SETTINGS
};

#define STATE_NONE 0
#define STATE_NEW 1
#define STATE_UPDATE 2
#define STATE_NOT_DOWNLOADED 3
#define STATE_DOWNLOADED 4
#define STATE_INSTALLED 5

#include "UserState.h"

static const char* const STORE_USER_STATE_PATH = "T:\\user_state.json";
//
//class Store
//{
//public:
//    Store();
//    ~Store();
//
//    HRESULT Initialize();
//    void SetDownloadProgress( uint32_t dlNow, uint32_t dlTotal );
//
//    // Data for StoreScene
//    StoreItem* GetItems() { return m_pItems; }
//    int GetItemCount() const { return m_nItemCount; }
//    int* GetFilteredIndices() { return m_pFilteredIndices; }
//    int GetFilteredCount() const { return m_nFilteredCount; }
//    int GetCurrentPage() const { return m_nApiPage * 3 + m_nDisplayPage + 1; }
//    int GetTotalPages() const;
//    int GetTotalCount() const { return m_nTotalCount; }
//    uint32_t GetDisplayState( StoreItem* pItem, int versionIndex ) const;
//    BOOL LoadAppsPage( int page, const char* categoryFilter, int selectedCategoryForCount = 0 );
//    void EnsureVersionsForItem( StoreItem* pItem );
//    void EnsureScreenshotForItem( StoreItem* pItem );
//    void MarkAppAsViewed( const char* appId );
//    void ProcessImageDownloader();
//    void SetVisibleRange( int start, int count );
//    void SetDisplayPage( int page ) { m_nDisplayPage = page; }
//    int GetDisplayPage() const { return m_nDisplayPage; }
//    void PreloadNextPage();
//    bool IsNextPageLoaded() const { return m_pNextItems != NULL && m_nNextItemCount > 0; }
//    bool HasNextPage() const { return m_hasNextPage; }
//    bool HasPreviousPage() const { return m_hasPreviousPage; }
//    bool AppendNextPage();
//    bool GoToNextPage();
//    bool GoToPrevPage( const char* categoryFilter );
//
//    void StartDownloadThread();
//    void BeginDownload( const std::string& versionId, const std::string& appName, const std::string& path );
//    void CloseDownloadThread();
//
//    const std::string& GetDownloadAppName() const { return m_downloadAppName; }
//    uint32_t GetDownloadNow() const { return m_downloadNow; }
//    uint32_t GetDownloadTotal() const { return m_downloadTotal; }
//    bool GetDownloadDone() const { return m_downloadDone; }
//    bool GetDownloadSuccess() const { return m_downloadSuccess; }
//    bool GetDownloadCancelRequested() const { return m_downloadCancelRequested; }
//    HANDLE GetDownloadThread() const { return m_downloadThread; }
//    void SetDownloadCancelRequested( bool v ) { m_downloadCancelRequested = v; }
//    UserState& GetUserState() { return m_userState; }
//    const char* GetUserStatePath() const { return STORE_USER_STATE_PATH; }
//
//private:
//    void SetVersionState( const char* appId, const char* version, int state );
//    BOOL HasUpdateAvailable( StoreItem* pItem ) const;
//    BOOL LoadCatalogFromWeb();
//    BOOL LoadCategoriesFromWeb();
//    //void BuildCategoryList();
//    void ReleaseAllIconsExceptVisible();
//    //void EnsureIconsForVisibleRange();
//    bool IsAppIdInVisibleRange( const std::string& appId ) const;
//    static bool OnIconLoadCompletion( void* ctx, LPDIRECT3DTEXTURE8* pOut, const std::string& filePath, const std::string& appId, LPDIRECT3DTEXTURE8 loadedTex );
//    void FreeNextPage();
//
//    UserState m_userState;
//    ImageDownloader m_imageDownloader;
//    StoreItem* m_pItems;
//    int m_nItemCount;
//    StoreItem* m_pNextItems;
//    int m_nNextItemCount;
//    int* m_pFilteredIndices;
//    int m_nFilteredCount;
//    int m_nApiPage;
//    int m_nDisplayPage;
//    int m_nTotalPages;
//    int m_nTotalCount;
//    bool m_hasNextPage;
//    bool m_hasPreviousPage;
//    bool m_nextPageHasNextPage;
//    bool m_nextPageHasPreviousPage;
//    std::string m_currentCategoryFilter;
//    int m_visibleStart;
//    int m_visibleCount;
//
//    std::string m_downloadVersionId;
//    std::string m_downloadAppName;
//    std::string m_downloadPath;
//    volatile uint32_t m_downloadNow;
//    volatile uint32_t m_downloadTotal;
//    volatile bool m_downloadDone;
//    bool m_downloadSuccess;
//    volatile bool m_downloadCancelRequested;
//    HANDLE m_downloadThread;
//    static DWORD WINAPI DownloadThreadProc( LPVOID param );
//};
//
//struct CUSTOMVERTEX
//{
//    FLOAT x, y, z, rhw;
//    DWORD color;
//};
//#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
//#define D3DFVF_TEXVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)
//
//#endif // STORE_H
