#pragma once

#include "Main.h"

// What to do with the downloaded zip after install completes
typedef enum AfterInstallAction
{
    AfterInstallDelete  = 0,
    AfterInstallKeep    = 1,
    AfterInstallAsk     = 2,
} AfterInstallAction;

// Flat binary struct persisted to T:\Settings.bin
// Add new fields at the END only to stay backwards-compatible
typedef struct
{
    char     downloadPath[256];
    uint32_t afterInstallAction;
} AppSettingsData;

class AppSettings
{
public:
    static bool Load();
    static bool Save();

    // Accessors
    static std::string        GetDownloadPath();
    static AfterInstallAction GetAfterInstallAction();

    // Mutators (call Save() to persist)
    static void SetDownloadPath(const std::string& path);
    static void SetAfterInstallAction(AfterInstallAction action);

private:
    static void            ApplyDefaults();
    static AppSettingsData mData;
    static bool            mLoaded;
};