#include "Debug.h"

void Debug::Print(const std::string format, ...)
{
    va_list args;
    va_start(args, format);
    int length = _vsnprintf(nullptr, 0, format.c_str(), args);
    if (length < 0)
    {
        va_end(args);
        return;
    }
    char* message = (char*)malloc(length + 1);
    _vsnprintf(message, length, format.c_str(), args);
    message[length] = 0;
    va_end(args);
    OutputDebugStringA(message);
    free(message);
}
