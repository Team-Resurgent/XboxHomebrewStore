#include "FileSystem.h"
#include "String.h"

#include <map>

namespace {
struct FileContainer {
    void* file;
};

uint32_t mMaxFileContainerID = 0;
std::map<uint32_t, FileContainer> mFileContainerMap;

uint32_t addFileContainer(FileContainer fileContainer) {
    uint32_t result = mMaxFileContainerID;
    mFileContainerMap[result] = fileContainer;
    mMaxFileContainerID++;
    return result;
}

FileContainer* getFileContainer(uint32_t fileHandle) {
    std::map<uint32_t, FileContainer>::iterator it = mFileContainerMap.find(fileHandle);
    if (it == mFileContainerMap.end()) {
        return nullptr;
    }
    return &it->second;
}

void deleteFileContainer(uint32_t fileHandle) {
    mFileContainerMap.erase(fileHandle);
}
} // namespace

bool FileSystem::FileGetFileInfoDetail(const std::string path, FileInfoDetail& out) {
    const DWORD invalidFileAttributes = (DWORD)0xFFFFFFFF;

    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == invalidFileAttributes) {
        return false;
    }

    out.path = path;
    out.isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    out.isFile = (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    out.accessTime.month = 1;
    out.accessTime.day = 1;
    out.accessTime.year = 2000;
    out.accessTime.hour = 0;
    out.accessTime.minute = 0;
    out.accessTime.second = 0;
    out.writeTime.month = 1;
    out.writeTime.day = 1;
    out.writeTime.year = 2000;
    out.writeTime.hour = 0;
    out.writeTime.minute = 0;
    out.writeTime.second = 0;

    HANDLE fileHandle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        out.isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        FILETIME fileTimeAccess;
        FILETIME fileTimeWrite;
        if (GetFileTime(fileHandle, NULL, &fileTimeAccess, &fileTimeWrite) == FALSE) {
            CloseHandle(fileHandle);
            return false;
        }

        SYSTEMTIME systemTimeAccessLocal;
        if (FileTimeToSystemTime(&fileTimeAccess, &systemTimeAccessLocal) == FALSE) {
            CloseHandle(fileHandle);
            return false;
        }

        out.accessTime.month = systemTimeAccessLocal.wMonth;
        out.accessTime.day = systemTimeAccessLocal.wDay;
        out.accessTime.year = systemTimeAccessLocal.wYear;
        out.accessTime.hour = systemTimeAccessLocal.wHour;
        out.accessTime.minute = systemTimeAccessLocal.wMinute;
        out.accessTime.second = systemTimeAccessLocal.wSecond;

        if (fileTimeWrite.dwHighDateTime != 0 || fileTimeWrite.dwLowDateTime) {
            FILETIME fileTimeWriteLocal;
            if (FileTimeToLocalFileTime(&fileTimeWrite, &fileTimeWriteLocal) == FALSE) {
                CloseHandle(fileHandle);
                return false;
            }

            SYSTEMTIME systemTimeWriteLocal;
            if (FileTimeToSystemTime(&fileTimeWriteLocal, &systemTimeWriteLocal) == FALSE) {
                CloseHandle(fileHandle);
                return false;
            }

            out.writeTime.month = systemTimeWriteLocal.wMonth;
            out.writeTime.day = systemTimeWriteLocal.wDay;
            out.writeTime.year = systemTimeWriteLocal.wYear;
            out.writeTime.hour = systemTimeWriteLocal.wHour;
            out.writeTime.minute = systemTimeWriteLocal.wMinute;
            out.writeTime.second = systemTimeAccessLocal.wSecond;
        }

        DWORD fileSize = GetFileSize(fileHandle, NULL);
        out.size = fileSize;

        CloseHandle(fileHandle);
    } else {
        out.size = 0;
    }
    return true;
}

std::vector<FileInfoDetail> FileSystem::FileGetFileInfoDetails(const std::string path) {
    std::vector<FileInfoDetail> fileInfoDetails;

    WIN32_FIND_DATAA findData;

    std::string searchPath = CombinePath(path, "*");
    HANDLE findHandle = FindFirstFileA(searchPath.c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        return fileInfoDetails;
    }

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        std::string currentPath = CombinePath(path, findData.cFileName);
        FileInfoDetail fileInfoDetail;
        if (!FileGetFileInfoDetail(currentPath, fileInfoDetail)) {
            FindClose(findHandle);
            return std::vector<FileInfoDetail>();
        }
        fileInfoDetails.push_back(fileInfoDetail);
    } while (FindNextFile(findHandle, &findData));
    FindClose(findHandle);

    for (size_t i = 0; i + 1 < fileInfoDetails.size(); i++) {
        for (size_t j = i + 1; j < fileInfoDetails.size(); j++) {
            if (_stricmp(fileInfoDetails[i].path.c_str(), fileInfoDetails[j].path.c_str()) > 0) {
                FileInfoDetail tmp = fileInfoDetails[i];
                fileInfoDetails[i] = fileInfoDetails[j];
                fileInfoDetails[j] = tmp;
            }
        }
    }

    return fileInfoDetails;
}

bool FileSystem::FileOpen(const std::string path, FileMode fileMode, uint32_t& fileHandle) {
    const char* access = "";
    if (fileMode == FileModeRead) {
        access = "rb";
    } else if (fileMode == FileModeWrite) {
        access = "wb";
    } else if (fileMode == FileModeAppend) {
        access = "ab";
    } else if (fileMode == FileModeReadUpdate) {
        access = "r+b";
    } else if (fileMode == FileModeWriteUpdate) {
        access = "w+b";
    } else if (fileMode == FileModeAppendUpdate) {
        access = "a+b";
    }
    FileContainer fileContainer;
    fileContainer.file = fopen(path.c_str(), access);
    if (fileContainer.file == NULL) {
        return false;
    }
    fileHandle = addFileContainer(fileContainer);
    return true;
}

bool FileSystem::FileRead(uint32_t fileHandle, char* readBuffer, uint32_t bytesToRead, uint32_t& bytesRead) {
    FileContainer* fileContainer = getFileContainer(fileHandle);
    if (fileContainer == NULL) {
        return false;
    }
    bytesRead = (uint32_t)fread(readBuffer, 1, bytesToRead, (FILE*)fileContainer->file);
    return true;
}

bool FileSystem::FileWrite(uint32_t fileHandle, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten) {
    FileContainer* fileContainer = getFileContainer(fileHandle);
    if (fileContainer == NULL) {
        bytesWritten = 0;
        return false;
    }
    bytesWritten = (uint32_t)fwrite(writeBuffer, 1, bytesToWrite, (FILE*)fileContainer->file);
    return bytesWritten == bytesToWrite;
}

bool FileSystem::FileWrite(const std::string path, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten) {
    uint32_t fileHandle;
    if (FileOpen(path, FileModeWrite, fileHandle) == false) {
        return false;
    }
    bool result = true;
    if (FileWrite(fileHandle, writeBuffer, bytesToWrite, bytesWritten) == false) {
        result = false;
    }
    return FileClose(fileHandle) && result;
}

bool FileSystem::FileClose(uint32_t fileHandle) {
    FileContainer* fileContainer = getFileContainer(fileHandle);
    if (fileContainer == NULL) {
        return false;
    }
    bool result = fclose((FILE*)fileContainer->file) == 0;
    deleteFileContainer(fileHandle);
    return result;
}

bool FileSystem::FileSeek(uint32_t fileHandle, FileSeekMode fileSeekMode, uint32_t offset) {
    int seek = SEEK_CUR;
    if (fileSeekMode == FileSeekModeStart) {
        seek = SEEK_SET;
    } else if (fileSeekMode == FileSeekModeEnd) {
        seek = SEEK_END;
    }
    FileContainer* fileContainer = getFileContainer(fileHandle);
    if (fileContainer == NULL) {
        return false;
    }
    FILE* file = (FILE*)fileContainer->file;
    fseek(file, offset, seek);
    return true;
}

bool FileSystem::FilePosition(uint32_t fileHandle, uint32_t& position) {
    FileContainer* fileContainer = getFileContainer(fileHandle);
    if (fileContainer == NULL) {
        return false;
    }
    FILE* file = (FILE*)fileContainer->file;
    position = ftell(file);
    return true;
}

bool FileSystem::FileTruncate(uint32_t fileHandle, uint32_t& position) {
    FileContainer* fileContainer = getFileContainer(fileHandle);
    if (fileContainer == NULL) {
        return false;
    }
    FILE* file = (FILE*)fileContainer->file;
    return _chsize(fileno(file), position) == 0;
}

bool FileSystem::SetFileTime(const std::string path, const FileTime fileTime) {
    SYSTEMTIME systemTime;
    systemTime.wYear = fileTime.year;
    systemTime.wMonth = fileTime.month;
    systemTime.wDay = fileTime.day;
    systemTime.wDayOfWeek = 0;
    systemTime.wHour = fileTime.hour;
    systemTime.wMinute = fileTime.minute;
    systemTime.wSecond = fileTime.second;
    systemTime.wMilliseconds = 0;

    FILETIME tempFileTime;
    SystemTimeToFileTime(&systemTime, &tempFileTime);

    HANDLE fileHandle = CreateFileA(path.c_str(), FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    ::SetFileTime(fileHandle, NULL, NULL, &tempFileTime);
    CloseHandle(fileHandle);
    return true;
}

bool FileSystem::DirectoryCreate(const std::string path) {
    bool exists;
    if (DirectoryExists(path, exists) == false) {
        return false;
    }
    if (exists) {
        return true;
    }
    return CreateDirectoryA(path.c_str(), NULL) == TRUE;
}

bool FileSystem::DirectoryDelete(const std::string path, bool recursive) {
    std::vector<FileInfoDetail> fileInfoDetails = FileGetFileInfoDetails(path);

    for (size_t i = 0; i < fileInfoDetails.size(); i++) {
        const FileInfoDetail& fileInfoDetail = fileInfoDetails[i];
        if (fileInfoDetail.isDirectory) {
            if (recursive && DirectoryDelete(fileInfoDetail.path, true) == false) {
                return false;
            }
        } else {
            if (FileDelete(fileInfoDetail.path) == false) {
                return false;
            }
        }
    }

    return RemoveDirectoryA(path.c_str()) == TRUE;
}

bool FileSystem::FileDelete(const std::string path) {
    return remove(path.c_str()) == 0;
}

bool FileSystem::FileCopy(const std::string sourcePath, const std::string destPath) {
    uint32_t sourceFileHandle;
    if (FileOpen(sourcePath, FileModeRead, sourceFileHandle) == false) {
        return false;
    }

    uint32_t destFileHandle;
    if (FileOpen(destPath, FileModeWrite, destFileHandle) == false) {
        FileClose(sourceFileHandle);
        return false;
    }

    char* buffer = (char*)malloc(32768);

    uint32_t bytesRead;
    if (FileRead(sourceFileHandle, buffer, 32768, bytesRead) == false) {
        free(buffer);
        FileClose(sourceFileHandle);
        FileClose(destFileHandle);
        return false;
    }

    while (bytesRead > 0) {
        uint32_t bytesWritten;
        if (FileWrite(destFileHandle, buffer, bytesRead, bytesWritten) == false || bytesWritten != bytesRead) {
            free(buffer);
            FileClose(sourceFileHandle);
            FileClose(destFileHandle);
            return false;
        }
        if (FileRead(sourceFileHandle, buffer, 32768, bytesRead) == false) {
            free(buffer);
            FileClose(sourceFileHandle);
            FileClose(destFileHandle);
            return false;
        }
    }

    free(buffer);

    FileClose(sourceFileHandle);
    FileClose(destFileHandle);
    return true;
}

bool FileSystem::FileMove(const std::string sourcePath, const std::string destPath) {
    BOOL result = MoveFileA(sourcePath.c_str(), destPath.c_str());
    return result == TRUE;
}

bool FileSystem::FileSize(uint32_t fileHandle, uint32_t& size) {
    FileContainer* fileContainer = getFileContainer(fileHandle);
    if (fileContainer == NULL) {
        return false;
    }
    FILE* file = (FILE*)fileContainer->file;
    uint32_t filePosition = ftell(file);
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, filePosition, SEEK_SET);
    return true;
}

bool FileSystem::FileExists(const std::string path, bool& exists) {
    FileInfoDetail fileInfoDetail;
    if (!FileGetFileInfoDetail(path, fileInfoDetail)) {
        exists = false;
        return true;
    }
    exists = fileInfoDetail.isFile;
    return true;
}

bool FileSystem::DirectoryExists(const std::string path, bool& exists) {
    FileInfoDetail fileInfoDetail;
    if (!FileGetFileInfoDetail(path, fileInfoDetail)) {
        exists = false;
        return true;
    }
    exists = fileInfoDetail.isDirectory;
    return true;
}

// Tested
std::string FileSystem::GetFileName(const std::string path) {
    size_t len = path.size();
    uint32_t length = 0;
    for (int i = (uint32_t)len; i > 0; i--) {
        if (path[i - 1] == '\\') {
            break;
        }
        length++;
    }
    return path.substr(len - length, length);
}

std::string FileSystem::GetRootPath(const std::string path) {
    if (path.empty()) {
        return "";
    }
    if (path[0] == '\\') {
        return path;
    }
    uint32_t length = 0;
    size_t pathLen = path.size();
    for (int i = 0; i < (int)pathLen; i++) {
        if (path[i] == ':') {
            break;
        }
        length++;
    }
    if (length + 1 >= pathLen)
        return "";
    return path.substr(length + 1, pathLen - length - 1);
}

std::string FileSystem::GetDriveLetter(const std::string path) {
    if (path.empty() || path[0] == '\\') {
        return "";
    }
    size_t length = 0;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == ':') {
            break;
        }
        length++;
    }
    return path.substr(0, length);
}

std::string FileSystem::GetFileNameWithoutExtension(const std::string path) {
    std::string fileName = GetFileName(path);
    std::string extension = GetExtension(fileName);
    size_t fnLen = fileName.size();
    size_t extLen = extension.size();
    if (extLen >= fnLen)
        return fileName;
    return String::Substring(fileName, 0, (int32_t)(fnLen - extLen));
}

// Tested
std::string FileSystem::GetExtension(const std::string path) {
    size_t pathLen = path.size();
    uint32_t length = 0;
    for (int i = (uint32_t)pathLen; i > 0; i--) {
        if (path[i - 1] == '.') {
            length++;
            break;
        }
        if (path[i - 1] == '\\') {
            break;
        }
        length++;
    }
    return path.substr(pathLen - length, length);
}

// Tested
std::string FileSystem::GetDirectory(const std::string path) {
    size_t pathLen = path.size();
    int32_t length = 0;
    for (int i = (uint32_t)pathLen; i > 0; i--) {
        if (path[i - 1] == '\\') {
            break;
        }
        length++;
    }
    length = length == 0 ? 0 : (int32_t)pathLen - length - 1;
    if (length <= 0) {
        return "";
    }
    return path.substr(0, (size_t)length);
}

// Tested
std::string FileSystem::CombinePath(const std::string first, const std::string second) {
    if (first.empty()) {
        return second;
    }
    if (second.empty()) {
        return first;
    }
    std::string trimmedFirst = String::RightTrim(first, '\\');
    std::string trimmedSecond = String::LeftTrim(second, '\\');
    return trimmedFirst + "\\" + trimmedSecond;
}