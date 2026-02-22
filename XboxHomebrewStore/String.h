#pragma once

#include "Main.h"

class String
{
public:
    static std::string Format(const std::string format, ...);
    static std::string FormatSize(uint32_t size);
    static std::string ToUpper(const std::string s);
};
