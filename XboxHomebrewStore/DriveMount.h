#pragma once

#include "External.h"

class DriveMount
{
public:
    static bool Init();
    static bool Mount(std::string driveName);
    static bool Unmount(std::string driveName);
};
