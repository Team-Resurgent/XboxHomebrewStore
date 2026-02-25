#pragma once

#include "Main.h"

typedef struct
{
    char appId[64];
} ViewSaveState;

class ViewState
{
public:
    static bool TrySave(const std::string appId);
    static bool GetViewed(const std::string appId);
};
