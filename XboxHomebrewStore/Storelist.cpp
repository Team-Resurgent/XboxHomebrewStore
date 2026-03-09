#include "StoreList.h"
#include "Debug.h"
#include "FileSystem.h"

// File format: 4-byte count, 4-byte activeIndex, then count *
// sizeof(StoreEntry)
#define STORE_LIST_HEADER_SIZE 8

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
    mActiveIndex = index;
    mStoreChanged = true;
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