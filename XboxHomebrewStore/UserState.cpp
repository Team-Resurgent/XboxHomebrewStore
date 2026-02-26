#include "UserState.h"
#include "FileSystem.h"
#include "Debug.h"

#define USER_STATE_PATH "T:\\UserState.bin"

bool UserState::TrySave(const std::string appId, const std::string versionId, const std::string* downloadPath, const std::string* installPath)
{
    uint32_t fileHandle = 0;
    if (FileSystem::FileOpen(USER_STATE_PATH, FileModeReadUpdate, fileHandle)) {
        UserSaveState existing;
        uint32_t bytesRead = 0;
        uint32_t recordIndex = 0;
        while (FileSystem::FileRead(fileHandle, (char*)&existing, sizeof(UserSaveState), bytesRead) && bytesRead == sizeof(UserSaveState)) {
            if (strcmp(existing.appId, appId.c_str()) == 0 && strcmp(existing.versionId, versionId.c_str()) == 0) {
                Debug::Print("Updating new userstate.\n");
                if (downloadPath != nullptr) {
                    strcpy(existing.downloadPath, downloadPath->c_str());
                }
                if (installPath != nullptr) {
                    strcpy(existing.installPath, installPath->c_str());
                } 
                uint32_t offset = recordIndex * sizeof(UserSaveState);
                FileSystem::FileSeek(fileHandle, FileSeekModeStart, offset);
                uint32_t bytesWritten = 0;
                bool ok = FileSystem::FileWrite(fileHandle, (char*)&existing, sizeof(UserSaveState), bytesWritten);
                FileSystem::FileClose(fileHandle);
                return ok && (bytesWritten == sizeof(UserSaveState));
            }
            recordIndex++;
        }
        FileSystem::FileClose(fileHandle);
    }

    if (!FileSystem::FileOpen(USER_STATE_PATH, FileModeAppend, fileHandle)) {
        return false;
    }

    Debug::Print("Saving new userstate.\n");

    UserSaveState userSaveState;
    memset(&userSaveState, 0, sizeof(UserSaveState));
    strcpy(userSaveState.appId, appId.c_str());
    strcpy(userSaveState.versionId, versionId.c_str());
    if (downloadPath != nullptr) {
        strcpy(userSaveState.downloadPath, downloadPath->c_str());
    }
    if (installPath != nullptr) {
        strcpy(userSaveState.installPath, installPath->c_str());
    }
    uint32_t bytesWritten = 0;
    bool ok = FileSystem::FileWrite(fileHandle, (char*)&userSaveState, sizeof(UserSaveState), bytesWritten);
    FileSystem::FileClose(fileHandle);
    return ok && (bytesWritten == sizeof(UserSaveState));
}

bool UserState::TryGetByAppId(const std::string appId, std::vector<UserSaveState>& out)
{
    out.clear();

    uint32_t fileHandle = 0;
    if (!FileSystem::FileOpen(USER_STATE_PATH, FileModeRead, fileHandle)) {
        return false;
    }

    UserSaveState state;
    uint32_t bytesRead = 0;
    while (FileSystem::FileRead(fileHandle, (char*)&state, sizeof(UserSaveState), bytesRead) && bytesRead == sizeof(UserSaveState)) {
        if (strcmp(state.appId, appId.c_str()) == 0) {
            out.push_back(state);
        }
    }

    FileSystem::FileClose(fileHandle);
    return true;
}

bool UserState::TryGetByAppIdAndVersionId(const std::string appId, const std::string versionId, UserSaveState& out)
{
    uint32_t fileHandle = 0;
    if (!FileSystem::FileOpen(USER_STATE_PATH, FileModeRead, fileHandle)) {
        return false;
    }

    UserSaveState state;
    uint32_t bytesRead = 0;
    while (FileSystem::FileRead(fileHandle, (char*)&state, sizeof(UserSaveState), bytesRead) && bytesRead == sizeof(UserSaveState)) {
        if (strcmp(state.appId, appId.c_str()) == 0 && strcmp(state.versionId, versionId.c_str()) == 0) {
            out = state;
            FileSystem::FileClose(fileHandle);
            return true;
        }
    }

    FileSystem::FileClose(fileHandle);
    return false;
}

bool UserState::PruneMissingPaths()
{
    uint32_t fileHandle = 0;
    if (!FileSystem::FileOpen(USER_STATE_PATH, FileModeReadUpdate, fileHandle)) {
        return false;
    }

    UserSaveState state;
    uint32_t bytesRead = 0;
    uint32_t recordIndex = 0;

    while (FileSystem::FileRead(fileHandle, (char*)&state, sizeof(UserSaveState), bytesRead) && bytesRead == sizeof(UserSaveState)) {
        bool changed = false;

        if (state.installPath[0] != '\0') {
            bool exists = false;
            if (FileSystem::DirectoryExists(state.installPath, exists) && !exists) {
                memset(state.installPath, 0, sizeof(state.installPath));
                changed = true;
            }
        }

        if (state.downloadPath[0] != '\0') {
            bool exists = false;
            if (FileSystem::FileExists(state.downloadPath, exists) && !exists) {
                memset(state.downloadPath, 0, sizeof(state.downloadPath));
                changed = true;
            }
        }

        if (changed) {
            FileSystem::FileSeek(fileHandle, FileSeekModeStart, recordIndex * sizeof(UserSaveState));
            uint32_t bytesWritten = 0;
            FileSystem::FileWrite(fileHandle, (char*)&state, sizeof(UserSaveState), bytesWritten);
        }

        recordIndex++;
    }

    FileSystem::FileClose(fileHandle);
    return true;
}
