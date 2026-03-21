#pragma once

#include "Main.h"
#include <string>

// LRU cache of D3D cover textures.
// Owns texture lifetime -- callers must NOT call Release() on returned pointers.
// Thread safety: all public methods must be called from the main/render thread only.
#define TEXTURE_CACHE_SIZE 32

class TextureCache {
public:
  static void Init();
  static void Shutdown();

  // Returns cached texture for appId, or NULL if not in cache.
  static D3DTexture *Get(const std::string &appId);

  // Inserts texture into cache. Cache takes ownership.
  // If cache is full, evicts the LRU entry (releases its texture).
  // If appId already present, updates it and refreshes recency.
  static void Put(const std::string &appId, D3DTexture *tex);

  // Removes a specific entry and releases its texture. No-op if not found.
  static void Remove(const std::string &appId);

  // Releases all textures and clears the cache.
  static void Clear();

private:
  typedef struct {
    std::string appId;
    D3DTexture *tex;
    uint32_t lastUsed; // frame counter -- higher = more recently used
  } CacheEntry;

  static CacheEntry mEntries[TEXTURE_CACHE_SIZE];
  static int32_t mCount;
  static uint32_t mFrame;
};