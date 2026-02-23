#pragma once

#include "Main.h"

class String
{
public:
    static std::string Format(const std::string format, ...);
    static std::string FormatSize(uint32_t size);
    static std::string ToUpper(const std::string s);
    static std::string LeftTrim(const std::string s, char ch);
    static std::string RightTrim(const std::string s, char ch);
    static std::string Substring(const std::string s, int32_t start, int32_t length);
    static bool EqualsIgnoreCase(const std::string a, const std::string b);
};
