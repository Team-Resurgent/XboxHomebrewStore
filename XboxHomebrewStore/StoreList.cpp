#include "StoreList.h"
#include "AppSettings.h"
#include "Debug.h"
#include "FileSystem.h"
#include "String.h"

// File format: 4-byte count, 4-byte activeIndex, then count *
// sizeof(StoreEntry)
#define STORE_LIST_HEADER_SIZE 8

// Returns the base cache directory based on user's setting:
//   T:\Cache  (default, temp drive)
//   D:\Cache  (app drive, always persistent)
//   <custom>  (user-defined)
static std::string GetCacheBase()
{
    CacheLocation loc = AppSettings::GetCacheLocation();
    if( loc == CacheLocationApp )
        return "D:\\Cache";
    if( loc == CacheLocationCustom )
    {
        std::string custom = AppSettings::GetCachePath();
        if( !custom.empty() ) return custom;
        // fallback to default if custom path is blank
    }
    return "T:\\Cache";
}

// ---------------------------------------------------------------------------
// CRC32 -- used to derive a stable folder name from the store URL
// ---------------------------------------------------------------------------
static uint32_t StoreCRC32( const void* data, size_t size )
{
    static uint32_t s_table[256];
    static int32_t  s_init = 0;
    if( !s_init )
    {
        for( uint32_t i = 0; i < 256; i++ )
        {
            uint32_t c = i;
            for( int32_t k = 0; k < 8; k++ )
                c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            s_table[i] = c;
        }
        s_init = 1;
    }
    uint32_t crc = 0xFFFFFFFFU;
    const uint8_t* p = (const uint8_t*)data;
    for( size_t i = 0; i < size; i++ )
        crc = s_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

// Normalise URL: lowercase + strip trailing slashes
static std::string NormaliseUrl( const std::string& url )
{
    std::string out = url;
    for( size_t i = 0; i < out.size(); i++ )
        out[i] = (char)tolower( (unsigned char)out[i] );
    while( !out.empty() && out[out.size()-1] == '/' )
        out.erase( out.size()-1, 1 );
    return out;
}

std::string StoreList::GetActiveCacheRoot()
{
    std::string url = NormaliseUrl( GetActiveUrl() );
    uint32_t crc = StoreCRC32( url.c_str(), url.size() );
    return String::Format( "%s\\%08X", GetCacheBase().c_str(), crc );
}

void StoreList::EnsureCacheDirs()
{
    std::string base = GetCacheBase();
    std::string root = GetActiveCacheRoot();
    FileSystem::DirectoryCreate( base.c_str() );
    FileSystem::DirectoryCreate( root.c_str() );
    FileSystem::DirectoryCreate( (root + "\\Covers").c_str() );
    FileSystem::DirectoryCreate( (root + "\\Screenshots").c_str() );
    FileSystem::DirectoryCreate( (root + "\\Meta").c_str() );

    // Write a human-readable identifier so the folder can be identified
    // when browsing manually via FTP -- not read by the app
    std::string infoPath = root + "\\store.txt";
    FILE* f = fopen( infoPath.c_str(), "wb" );
    if( f )
    {
        std::string url = GetActiveUrl();
        StoreEntry* e = GetEntry( mActiveIndex );
        std::string name = e ? std::string( e->name ) : url;
        fwrite( name.c_str(), 1, name.size(), f );
        fwrite( "\r\n", 1, 2, f );
        fwrite( url.c_str(), 1, url.size(), f );
        fwrite( "\r\n", 1, 2, f );
        fclose( f );
    }
}

StoreEntry StoreList::mEntries[STORE_LIST_MAX];
int32_t StoreList::mCount = 0;
int32_t StoreList::mActiveIndex = 0;
bool StoreList::mLoaded = false;
bool StoreList::mStoreChanged = false;

// ==========================================================================
void StoreList::ApplyDefaults() {
  mCount = 1;
  mActiveIndex = 0;
  memset(mEntries, 0, sizeof(mEntries));
  strncpy(mEntries[0].name, DEFAULT_STORE_NAME, STORE_NAME_MAX - 1);
  strncpy(mEntries[0].url, DEFAULT_STORE_URL, STORE_URL_MAX - 1);
}

// ==========================================================================
bool StoreList::Load() {
  ApplyDefaults();

  uint32_t fileHandle = 0;
  if (!FileSystem::FileOpen(STORE_LIST_PATH, FileModeRead, fileHandle)) {
    Debug::Print("StoreList: no file, using defaults.\n");
    mLoaded = true;
    return false;
  }

  uint32_t fileSize = 0;
  FileSystem::FileSize(fileHandle, fileSize);

  if (fileSize < STORE_LIST_HEADER_SIZE) {
    FileSystem::FileClose(fileHandle);
    mLoaded = true;
    return false;
  }

  // Read header: count + activeIndex
  uint32_t header[2] = {0, 0};
  uint32_t bytesRead = 0;
  if (!FileSystem::FileRead(fileHandle, (char *)header, STORE_LIST_HEADER_SIZE, bytesRead) ||
      bytesRead != STORE_LIST_HEADER_SIZE) {
    FileSystem::FileClose(fileHandle);
    mLoaded = true;
    return false;
  }

  int32_t count = (int32_t)header[0];
  int32_t activeIndex = (int32_t)header[1];

  if (count < 1 || count > STORE_LIST_MAX) {
    FileSystem::FileClose(fileHandle);
    mLoaded = true;
    return false;
  }

  memset(mEntries, 0, sizeof(mEntries));
  uint32_t toRead = (uint32_t)(count * sizeof(StoreEntry));
  bytesRead = 0;
  bool ok = FileSystem::FileRead(fileHandle, (char *)mEntries, toRead, bytesRead);
  FileSystem::FileClose(fileHandle);

  if (!ok || bytesRead != toRead) {
    ApplyDefaults();
    mLoaded = true;
    return false;
  }

  // Null-terminate all strings defensively
  for (int32_t i = 0; i < count; i++) {
    mEntries[i].name[STORE_NAME_MAX - 1] = '\0';
    mEntries[i].url[STORE_URL_MAX - 1] = '\0';
  }

  mCount = count;
  mActiveIndex = (activeIndex >= 0 && activeIndex < count) ? activeIndex : 0;

  mLoaded = true;
  Debug::Print("StoreList: loaded %d entries.\n", mCount);
  return true;
}

// ==========================================================================
bool StoreList::Save() {
  uint32_t fileHandle = 0;
  if (!FileSystem::FileOpen(STORE_LIST_PATH, FileModeWrite, fileHandle)) {
    return false;
  }

  // Write header
  uint32_t header[2] = {(uint32_t)mCount, (uint32_t)mActiveIndex};
  uint32_t bytesWritten = 0;
  bool ok = FileSystem::FileWrite(fileHandle, (char *)header, STORE_LIST_HEADER_SIZE, bytesWritten);

  if (ok && bytesWritten == STORE_LIST_HEADER_SIZE) {
    uint32_t toWrite = (uint32_t)(mCount * sizeof(StoreEntry));
    bytesWritten = 0;
    ok = FileSystem::FileWrite(fileHandle, (char *)mEntries, toWrite, bytesWritten);
    ok = ok && (bytesWritten == toWrite);
  }

  FileSystem::FileClose(fileHandle);
  if (ok) {
    Debug::Print("StoreList: saved %d entries.\n", mCount);
  } else {
    Debug::Print("StoreList: save failed.\n");
  }
  return ok;
}

// ==========================================================================
int32_t StoreList::GetCount() {
  if (!mLoaded) {
    Load();
  }
  return mCount;
}

StoreEntry *StoreList::GetEntry(int32_t index) {
  if (!mLoaded) {
    Load();
  }
  if (index < 0 || index >= mCount) {
    return nullptr;
  }
  return &mEntries[index];
}

int32_t StoreList::GetActiveIndex() {
  if (!mLoaded) {
    Load();
  }
  return mActiveIndex;
}

void StoreList::SetActiveIndex(int32_t index) {
  if (!mLoaded) {
    Load();
  }
  if (index >= 0 && index < mCount && index != mActiveIndex) {
    // Capture and purge the old store's meta cache before switching
    std::string oldMetaDir = GetActiveCacheRoot() + "\\Meta";

    mActiveIndex = index;
    mStoreChanged = true;

    // Delete old meta files -- they belong to the previous store
    bool exists = false;
    FileSystem::DirectoryExists(oldMetaDir.c_str(), exists);
    if (exists) {
      FileSystem::DirectoryDelete(oldMetaDir.c_str(), true);
      FileSystem::DirectoryCreate(oldMetaDir.c_str());
      Debug::Print("StoreList: purged old store meta %s\n", oldMetaDir.c_str());
    }
  }
}

std::string StoreList::GetActiveUrl() {
  if (!mLoaded) {
    Load();
  }
  if (mCount == 0) {
    return DEFAULT_STORE_URL;
  }
  return std::string(mEntries[mActiveIndex].url);
}

int32_t StoreList::Add(const std::string &name, const std::string &url) {
  if (!mLoaded) {
    Load();
  }
  if (mCount >= STORE_LIST_MAX) {
    return -1;
  }

  int32_t idx = mCount++;
  memset(&mEntries[idx], 0, sizeof(StoreEntry));
  strncpy(mEntries[idx].name, name.c_str(), STORE_NAME_MAX - 1);
  strncpy(mEntries[idx].url, url.c_str(), STORE_URL_MAX - 1);
  return idx;
}

bool StoreList::Edit(int32_t index, const std::string &name, const std::string &url) {
  if (!mLoaded) {
    Load();
  }
  if (index < 0 || index >= mCount) {
    return false;
  }

  strncpy(mEntries[index].name, name.c_str(), STORE_NAME_MAX - 1);
  mEntries[index].name[STORE_NAME_MAX - 1] = '\0';
  strncpy(mEntries[index].url, url.c_str(), STORE_URL_MAX - 1);
  mEntries[index].url[STORE_URL_MAX - 1] = '\0';

  // If the edited entry is the active store, trigger a reload in StoreScene
  if (index == mActiveIndex) {
    mStoreChanged = true;
  }

  return true;
}

bool StoreList::Delete(int32_t index) {
  if (!mLoaded) {
    Load();
  }
  if (index < 0 || index >= mCount) {
    return false;
  }
  if (mCount == 1) {
    return false; // always keep at least one
  }

  // Shift entries down
  for (int32_t i = index; i < mCount - 1; i++) {
    mEntries[i] = mEntries[i + 1];
  }
  mCount--;

  // Adjust active index
  if (mActiveIndex >= mCount) {
    mActiveIndex = mCount - 1;
  } else if (mActiveIndex > index) {
    mActiveIndex--;
  }

  return true;
}

bool StoreList::WasStoreChanged() { return mStoreChanged; }

void StoreList::ClearStoreChangedFlag() { mStoreChanged = false; }