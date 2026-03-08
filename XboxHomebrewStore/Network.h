#pragma once

#include "Main.h"

class Network
{
public:
    static int         Init();
    static const char* GetIP();

private:
    static char mIP[16];  // "xxx.xxx.xxx.xxx\0"
};