#pragma once

#include "Main.h"

class String
{
public:
    static std::string Format(const std::string format, ...);
    static std::string FormatSize(uint32_t size);
    static std::string ToUpper(const std::string s);
    static std::string ToLower(const std::string s);
    static std::string LeftTrim(const std::string s, char ch);
    static std::string RightTrim(const std::string s, char ch);
    static std::string Substring(const std::string s, int32_t start, int32_t length);
    static bool EqualsIgnoreCase(const std::string a, const std::string b);
    static bool EndsWith(const std::string value, const std::string ending);
    // Returns true if candidate (e.g. "v1.2.0") is newer than current (e.g. "v1.0.0")
    static bool IsNewerVersion(const std::string& current, const std::string& candidate);
};