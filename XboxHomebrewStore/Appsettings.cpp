#include "AppSettings.h"
#include "FileSystem.h"
#include "Debug.h"

#define SETTINGS_PATH "T:\\Settings.bin"

#define DEFAULT_DOWNLOAD_PATH  "HDD0-E:\\Homebrew\\Downloads\\"
#define DEFAULT_INSTALL_PATH   "HDD0-E:\\Homebrew\\Installs\\"

AppSettingsData AppSettings::mData;
bool            AppSettings::mLoaded = false;

// ==========================================================================
void AppSettings::ApplyDefaults()
{
    memset(&mData, 0, sizeof(AppSettingsData));
    strncpy(mData.downloadPath,   DEFAULT_DOWNLOAD_PATH,  sizeof(mData.downloadPath)  - 1);
    strncpy(mData.lastInstallPath, DEFAULT_INSTALL_PATH,  sizeof(mData.lastInstallPath) - 1);
    mData.afterInstallAction = (uint32_t)AfterInstallDelete;
}

// ==========================================================================
bool AppSettings::Load()
{
    ApplyDefaults();

    uint32_t fileHandle = 0;
    if (!FileSystem::FileOpen(SETTINGS_PATH, FileModeRead, fileHandle))
    {
        Debug::Print("AppSettings: no settings file, using defaults.\n");
        mLoaded = true;
        return false;
    }

    uint32_t fileSize = 0;
    FileSystem::FileSize(fileHandle, fileSize);

    // Only read up to sizeof(AppSettingsData) — handles older smaller files
    uint32_t readSize = fileSize < sizeof(AppSettingsData) ? fileSize : sizeof(AppSettingsData);
    uint32_t bytesRead = 0;
    bool ok = FileSystem::FileRead(fileHandle, (char*)&mData, readSize, bytesRead);
    FileSystem::FileClose(fileHandle);

    if (!ok || bytesRead == 0)
    {
        ApplyDefaults();
        mLoaded = true;
        return false;
    }

    // Null-terminate string fields defensively
    mData.downloadPath[sizeof(mData.downloadPath)     - 1] = '\0';
    mData.lastInstallPath[sizeof(mData.lastInstallPath) - 1] = '\0';

    // If paths were blank (older file), apply defaults for those fields
    if (mData.downloadPath[0] == '\0')
        strncpy(mData.downloadPath, DEFAULT_DOWNLOAD_PATH, sizeof(mData.downloadPath) - 1);
    if (mData.lastInstallPath[0] == '\0')
        strncpy(mData.lastInstallPath, DEFAULT_INSTALL_PATH, sizeof(mData.lastInstallPath) - 1);

    // Clamp enum value
    if (mData.afterInstallAction > (uint32_t)AfterInstallAsk)
        mData.afterInstallAction = (uint32_t)AfterInstallDelete;

    mLoaded = true;
    Debug::Print("AppSettings: loaded.\n");
    return true;
}

// ==========================================================================
bool AppSettings::Save()
{
    uint32_t bytesWritten = 0;
    bool ok = FileSystem::FileWrite(SETTINGS_PATH, (char*)&mData,
                                    sizeof(AppSettingsData), bytesWritten);
    if (!ok || bytesWritten != sizeof(AppSettingsData))
    {
        Debug::Print("AppSettings: save failed.\n");
        return false;
    }
    Debug::Print("AppSettings: saved.\n");
    return true;
}

// ==========================================================================
// Accessors
// ==========================================================================
std::string AppSettings::GetDownloadPath()
{
    if (!mLoaded) Load();
    return std::string(mData.downloadPath);
}

std::string AppSettings::GetLastInstallPath()
{
    if (!mLoaded) Load();
    return std::string(mData.lastInstallPath);
}

AfterInstallAction AppSettings::GetAfterInstallAction()
{
    if (!mLoaded) Load();
    return (AfterInstallAction)mData.afterInstallAction;
}

// ==========================================================================
// Mutators
// ==========================================================================
void AppSettings::SetDownloadPath(const std::string& path)
{
    if (!mLoaded) Load();
    strncpy(mData.downloadPath, path.c_str(), sizeof(mData.downloadPath) - 1);
    mData.downloadPath[sizeof(mData.downloadPath) - 1] = '\0';
}

void AppSettings::SetLastInstallPath(const std::string& path)
{
    if (!mLoaded) Load();
    strncpy(mData.lastInstallPath, path.c_str(), sizeof(mData.lastInstallPath) - 1);
    mData.lastInstallPath[sizeof(mData.lastInstallPath) - 1] = '\0';
}

void AppSettings::SetAfterInstallAction(AfterInstallAction action)
{
    if (!mLoaded) Load();
    mData.afterInstallAction = (uint32_t)action;
}