#pragma once

#include "Main.h"

typedef enum FileMode {
    FileModeRead = 0,
    FileModeWrite = 1,
    FileModeAppend = 2,
    FileModeReadUpdate = 3,
    FileModeWriteUpdate = 4,
    FileModeAppendUpdate = 5
} FileMode;

typedef enum FileSeekMode {
    FileSeekModeStart = 0,
    FileSeekModeEnd = 1,
    FileSeekModeCurrent = 2
} FileSeekMode;

typedef struct FileTime {
    uint16_t month;
    uint16_t day;
    uint16_t year;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
} FileTime;

typedef struct FileInfoDetail {
    bool isDirectory;
    bool isFile;
    bool isVirtual;
    std::string path;
    uint32_t size;
    FileTime accessTime;
    FileTime writeTime;

    FileInfoDetail() {
        isDirectory = false;
        isFile = false;
        isVirtual = false;
        size = 0;
        memset(&accessTime, 0, sizeof(FileTime));
        memset(&writeTime, 0, sizeof(FileTime));
    }
} FileInfoDetail;

class FileSystem {
  public:
    static bool FileGetFileInfoDetail(const std::string path, FileInfoDetail& out);
    static std::vector<FileInfoDetail> FileGetFileInfoDetails(const std::string path);

    static bool FileOpen(const std::string path, FileMode fileMode, uint32_t& fileHandle);
    static bool FileRead(uint32_t fileHandle, char* readBuffer, uint32_t bytesToRead, uint32_t& bytesRead);
    static bool FileWrite(uint32_t fileHandle, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten);
    static bool FileWrite(const std::string path, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten);
    static bool FileClose(uint32_t fileHandle);
    static bool FileSeek(uint32_t fileHandle, FileSeekMode fileSeekMode, uint32_t offset);
    static bool FilePosition(uint32_t fileHandle, uint32_t& position);
    static bool FileTruncate(uint32_t fileHandle, uint32_t& position);
    static bool SetFileTime(const std::string path, const FileTime fileTime);

    static bool DirectoryCreate(const std::string path);
    static bool DirectoryDelete(const std::string path, bool recursive);
    static bool FileDelete(const std::string path);
    static bool FileCopy(const std::string sourcePath, const std::string destPath);
    static bool FileMove(const std::string sourcePath, const std::string destPath);
    static bool FileSize(uint32_t fileHandle, uint32_t& size);
    static bool FileExists(const std::string path, bool& exists);
    static bool DirectoryExists(const std::string path, bool& exists);

    static std::string GetDriveLetter(const std::string path);
    static std::string GetRootPath(const std::string path);
    static std::string GetFileName(const std::string path);
    static std::string GetFileNameWithoutExtension(const std::string path);
    static std::string GetExtension(const std::string path);
    static std::string GetDirectory(const std::string path);
    static std::string CombinePath(const std::string first, const std::string second);
};
