#pragma once

#include "Main.h"

class String
{
public:
    static std::string Format(const char* format, ...);
    static std::string FormatSize(uint32_t size) ;
};
