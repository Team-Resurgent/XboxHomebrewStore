#pragma once

#include "External.h"

#include <string>

class DriveMount
{
public:
    static bool Mount(std::string driveName);
    static bool Unmount(std::string driveName);
};
