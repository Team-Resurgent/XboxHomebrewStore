#include "ViewState.h"
#include "FileSystem.h"
#include "Debug.h"

#define VIEW_STATE_PATH "T:\\ViewState.bin"

bool ViewState::TrySave(const std::string appId, const std::string verId)
{
    uint32_t fileHandle = 0;
    if (FileSystem::FileOpen(VIEW_STATE_PATH, FileModeReadUpdate, fileHandle)) {
        uint32_t fileSize = 0;
        if (FileSystem::FileSize(fileHandle, fileSize)) {
            const uint32_t recordSize = sizeof(ViewSaveState);
            uint32_t recordCount = fileSize / recordSize;
            ViewSaveState existing;

            for (uint32_t recordIndex = 0; recordIndex < recordCount; recordIndex++) {
                FileSystem::FileSeek(fileHandle, FileSeekModeStart, recordIndex * recordSize);
                uint32_t bytesRead = 0;
                if (!FileSystem::FileRead(fileHandle, (char*)&existing, recordSize, bytesRead) || bytesRead != recordSize) {
                    continue;
                }
                if (strcmp(existing.appId, appId.c_str()) == 0) {
                    Debug::Print("Updating viewstate.\n");
                    strcpy(existing.verId, verId.c_str());
                    FileSystem::FileSeek(fileHandle, FileSeekModeStart, recordIndex * recordSize);
                    uint32_t bytesWritten = 0;
                    bool ok = FileSystem::FileWrite(fileHandle, (char*)&existing, recordSize, bytesWritten);
                    FileSystem::FileClose(fileHandle);
                    return ok && (bytesWritten == recordSize);
                }
            }
        }
        FileSystem::FileClose(fileHandle);
    }

    if (!FileSystem::FileOpen(VIEW_STATE_PATH, FileModeAppend, fileHandle)) {
        return false;
    }

    Debug::Print("Saving new viewstate.\n");

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
    if (!FileSystem::FileOpen(VIEW_STATE_PATH, FileModeRead, fileHandle)) {
        return false;
    }

    uint32_t fileSize = 0;
    if (!FileSystem::FileSize(fileHandle, fileSize)) {
        FileSystem::FileClose(fileHandle);
        return false;
    }

    const uint32_t recordSize = sizeof(ViewSaveState);
    uint32_t recordCount = fileSize / recordSize;
    ViewSaveState existing;

    for (uint32_t i = 0; i < recordCount; i++) {
        FileSystem::FileSeek(fileHandle, FileSeekModeStart, i * recordSize);
        uint32_t bytesRead = 0;
        if (!FileSystem::FileRead(fileHandle, (char*)&existing, recordSize, bytesRead) || bytesRead != recordSize) {
            continue;
        }
        if (strcmp(existing.appId, appId.c_str()) == 0) {
            FileSystem::FileClose(fileHandle);
            return true;
        }
    }

    FileSystem::FileClose(fileHandle);
    return false;
}
