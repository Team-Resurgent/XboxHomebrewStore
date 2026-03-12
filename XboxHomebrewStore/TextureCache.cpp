#include "TextureCache.h"
#include "Debug.h"

TextureCache::CacheEntry TextureCache::mEntries[TEXTURE_CACHE_SIZE];
int32_t TextureCache::mCount = 0;
uint32_t TextureCache::mFrame = 0;

void TextureCache::Init() {
  int32_t i;
  for (i = 0; i < TEXTURE_CACHE_SIZE; i++) {
    mEntries[i].appId = "";
    mEntries[i].tex = NULL;
    mEntries[i].lastUsed = 0;
  }
  mCount = 0;
  mFrame = 0;
}

void TextureCache::Shutdown() {
  Clear();
}

D3DTexture *TextureCache::Get(const std::string &appId) {
  int32_t i;
  mFrame++;
  for (i = 0; i < mCount; i++) {
    if (mEntries[i].appId == appId) {
      mEntries[i].lastUsed = mFrame;
      return mEntries[i].tex;
    }
  }
  return NULL;
}

void TextureCache::Put(const std::string &appId, D3DTexture *tex) {
  int32_t i;
  int32_t lruIndex;
  uint32_t lruTime;

  if (tex == NULL) {
    return;
  }

  // Update existing entry if present
  for (i = 0; i < mCount; i++) {
    if (mEntries[i].appId == appId) {
      if (mEntries[i].tex != NULL && mEntries[i].tex != tex) {
        mEntries[i].tex->Release();
      }
      mEntries[i].tex = tex;
      mEntries[i].lastUsed = ++mFrame;
      return;
    }
  }

  // Add new entry if there's room
  if (mCount < TEXTURE_CACHE_SIZE) {
    mEntries[mCount].appId = appId;
    mEntries[mCount].tex = tex;
    mEntries[mCount].lastUsed = ++mFrame;
    mCount++;
    return;
  }

  // Cache full -- evict LRU entry
  lruIndex = 0;
  lruTime = mEntries[0].lastUsed;
  for (i = 1; i < mCount; i++) {
    if (mEntries[i].lastUsed < lruTime) {
      lruTime = mEntries[i].lastUsed;
      lruIndex = i;
    }
  }
  if (mEntries[lruIndex].tex != NULL) {
    mEntries[lruIndex].tex->Release();
  }
  mEntries[lruIndex].appId = appId;
  mEntries[lruIndex].tex = tex;
  mEntries[lruIndex].lastUsed = ++mFrame;
}

void TextureCache::Clear() {
  int32_t i;
  for (i = 0; i < mCount; i++) {
    if (mEntries[i].tex != NULL) {
      mEntries[i].tex->Release();
      mEntries[i].tex = NULL;
    }
    mEntries[i].appId = "";
    mEntries[i].lastUsed = 0;
  }
  mCount = 0;
}