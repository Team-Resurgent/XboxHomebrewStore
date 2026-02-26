#include "ViewState.h"
#include "FileSystem.h"

#define VIEW_STATE_PATH "T:\\ViewState.bin"

bool ViewState::TrySave(const std::string appId, const std::string verId)
{
    uint32_t fileHandle = 0;
    if (FileSystem::FileOpen(VIEW_STATE_PATH, FileModeReadUpdate, fileHandle)) {
        ViewSaveState existing;
        uint32_t bytesRead = 0;
        uint32_t recordIndex = 0;
        while (FileSystem::FileRead(fileHandle, (char*)&existing, sizeof(ViewSaveState), bytesRead) && bytesRead == sizeof(ViewSaveState)) {
            if (strcmp(existing.appId, appId.c_str()) == 0) {
                strcpy(existing.verId, verId.c_str());
                uint32_t offset = recordIndex * sizeof(ViewSaveState);
                FileSystem::FileSeek(fileHandle, FileSeekModeStart, offset);
                uint32_t bytesWritten = 0;
                bool ok = FileSystem::FileWrite(fileHandle, (char*)&existing, sizeof(ViewSaveState), bytesWritten);
                FileSystem::FileClose(fileHandle);
                return ok && (bytesWritten == sizeof(ViewSaveState));
            }
            recordIndex++;
        }
        FileSystem::FileClose(fileHandle);
    }

    if (!FileSystem::FileOpen(VIEW_STATE_PATH, FileModeAppend, fileHandle)) {
        return false;
    }

    ViewSaveState viewSaveState;
    memset(&viewSaveState, 0, sizeof(ViewSaveState));
    strcpy(viewSaveState.appId, appId.c_str());
    strcpy(viewSaveState.verId, verId.c_str());

    uint32_t bytesWritten = 0;
    bool ok = FileSystem::FileWrite(fileHandle, (char*)&viewSaveState, sizeof(ViewSaveState), bytesWritten);
    FileSystem::FileClose(fileHandle);
    return ok && (bytesWritten == sizeof(ViewSaveState));
}

bool ViewState::GetViewed(const std::string appId, const std::string verId)
{
    uint32_t fileHandle = 0;
    if (FileSystem::FileOpen(VIEW_STATE_PATH, FileModeReadUpdate, fileHandle)) {
        ViewSaveState existing;
        uint32_t bytesRead = 0;
        uint32_t recordIndex = 0;
        while (FileSystem::FileRead(fileHandle, (char*)&existing, sizeof(ViewSaveState), bytesRead) && bytesRead == sizeof(ViewSaveState)) {
            if (strcmp(existing.appId, appId.c_str()) == 0) {
                return true;
            }
            recordIndex++;
        }
        FileSystem::FileClose(fileHandle);
    }
    return false;
}
