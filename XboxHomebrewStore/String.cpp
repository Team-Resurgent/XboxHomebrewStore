#include "String.h"

std::string String::Format(const std::string format, ...)
{
    va_list args;
    va_start(args, format);
    int length = _vsnprintf(nullptr, 0, format.c_str(), args);
    if (length < 0)
    {
        va_end(args);
        return "";
    }
    char* message = (char*)malloc(length + 1);
    _vsnprintf(message, length, format.c_str(), args);
    message[length] = 0;
    va_end(args);
    std::string result(message);
    free(message);
    return result;
}

std::string String::FormatSize(uint32_t size) 
{
    const uint32_t KB = 1024;
    const uint32_t MB = KB * KB;
    if (size < KB) {
        return Format("%luB", size);
    } else if (size < MB) {
        return Format("%luKB", size / KB);
    } else {
        return Format("%luMB", size / MB);
    }
}

std::string String::ToUpper(const std::string s)
{
    std::string r = s;
    for (size_t i = 0; i < r.length(); i++)
    {
        r[i] = (char)toupper((unsigned char)r[i]);
    }
    return r;
}

std::string String::LeftTrim(const std::string s, char ch)
{
    size_t start = 0;
    while (start < s.size() && s[start] == ch) {
        start++;
    }
    return s.substr(start, std::string::npos);
}

std::string String::RightTrim(const std::string s, char ch)
{
    size_t end = s.size();
    while (end > 0 && s[end - 1] == ch) {
        end--;
    }
    return s.substr(0, end);
}

std::string String::Substring(const std::string s, int32_t start, int32_t length)
{
    if (start < 0) {
        start = 0;
    }
    if (length < 0) {
        length = 0;
    }
    if ((size_t)start >= s.size()) {
        return "";
    }
    size_t maxLen = s.size() - (size_t)start;
    if ((size_t)length > maxLen) {
        length = (int32_t)maxLen;
    }
    return s.substr((size_t)start, (size_t)length);
}

bool String::EqualsIgnoreCase(const std::string a, const std::string b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++)
    {
        if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i]))
            return false;
    }
    return true;
}