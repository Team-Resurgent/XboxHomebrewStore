#include "String.h"
#include <stdarg.h>
#include <stdlib.h>

std::string String::Format(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int length = _vscprintf(format, args);
    if (length < 0)
    {
        va_end(args);
        return std::string();
    }
    char* buf = (char*)malloc(length + 1);
    if (buf == NULL)
    {
        va_end(args);
        return std::string();
    }
    _vsnprintf(buf, length + 1, format, args);
    va_end(args);
    buf[length] = '\0';
    std::string result(buf);
    free(buf);
    return result;
}
