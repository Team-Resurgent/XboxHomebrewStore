#pragma once

#include "External.h"

#include <string>
#include <vector>

class DriveMount
{
public:
    static bool Init();
    static bool Mount(std::string driveName);
    static bool Unmount(std::string driveName);
    static std::string MapFtpPath(const std::string path);
    static bool FtpPathMounted(const std::string path);
    static bool GetTotalNumberOfBytes(const std::string mountPoint, uint64_t& totalSize);
    static bool GetTotalFreeNumberOfBytes(const std::string mountPoint, uint64_t& totalFree);
    static std::vector<std::string> GetMountedDrives();
};
