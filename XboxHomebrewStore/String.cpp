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