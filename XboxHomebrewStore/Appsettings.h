#pragma once

#include "Main.h"

// What to do with the downloaded zip after install completes
typedef enum AfterInstallAction
{
    AfterInstallDelete  = 0,   // Always delete  (default)
    AfterInstallKeep    = 1,   // Always keep
    AfterInstallAsk     = 2,   // Ask each time
} AfterInstallAction;

// Flat binary struct persisted to T:\Settings.bin
// Add new fields at the END only to stay backwards-compatible
typedef struct
{
    char              downloadPath[256];      // scratch folder for zips
    char              lastInstallPath[256];   // pre-fill for InstallPathScene
    uint32_t          afterInstallAction;     // AfterInstallAction enum value
    uint8_t           _padding[60];           // reserved for future fields
} AppSettingsData;

class AppSettings
{
public:
    // Load from disk into memory.  Safe to call multiple times.
    // Returns true if loaded, false if file missing (defaults applied).
    static bool Load();

    // Persist current in-memory settings to disk.
    static bool Save();

    // Accessors
    static std::string      GetDownloadPath();
    static std::string      GetLastInstallPath();
    static AfterInstallAction GetAfterInstallAction();

    // Mutators  (call Save() to persist)
    static void SetDownloadPath(const std::string& path);
    static void SetLastInstallPath(const std::string& path);
    static void SetAfterInstallAction(AfterInstallAction action);

private:
    static void          ApplyDefaults();
    static AppSettingsData mData;
    static bool            mLoaded;
};