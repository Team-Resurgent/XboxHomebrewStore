#pragma once

#include "Main.h"

#define STORE_LIST_PATH      "T:\\Stores.bin"
#define STORE_NAME_MAX       64
#define STORE_URL_MAX        256
#define STORE_LIST_MAX       16
#define DEFAULT_STORE_NAME   "Xbox Homebrew Store"
#define DEFAULT_STORE_URL    "https://api.xboxhomebrew.store"

typedef struct
{
    char name[STORE_NAME_MAX];
    char url[STORE_URL_MAX];
} StoreEntry;

// ---------------------------------------------------------------------------
// StoreList  –  manages the list of configured store URLs
//
// Persisted to T:\Stores.bin as a flat array of StoreEntry structs.
// The active store index is stored in AppSettings.
// WebManager reads the active URL via StoreList::GetActiveUrl().
// ---------------------------------------------------------------------------
class StoreList
{
public:
    static bool Load();
    static bool Save();

    static int32_t     GetCount();
    static StoreEntry* GetEntry(int32_t index);

    static int32_t GetActiveIndex();
    static void    SetActiveIndex(int32_t index);

    static std::string GetActiveUrl();

    // Returns true if the active store was changed since the last call to
    // ClearStoreChangedFlag().  Used by StoreScene to detect reload needed.
    static bool WasStoreChanged();
    static void ClearStoreChangedFlag();

    // Returns new index, or -1 on failure (list full)
    static int32_t Add(const std::string& name, const std::string& url);

    // Edit in place
    static bool Edit(int32_t index, const std::string& name, const std::string& url);

    // Delete by index — shifts remaining entries down
    static bool Delete(int32_t index);

private:
    static void ApplyDefaults();

    static StoreEntry mEntries[STORE_LIST_MAX];
    static int32_t    mCount;
    static int32_t    mActiveIndex;
    static bool       mLoaded;
    static bool       mStoreChanged;  // set when active index changes
};