#pragma once

#include "Main.h"

typedef struct
{
    char appId[64];
    char versionId[64];
    char downloadPath[256];
    char installPath[256];
} UserSaveState;

class UserState
{
public:
    static bool TrySave(const std::string appId, const std::string versionId, const std::string* downloadPath, const std::string* installPath);
    static bool TryGetByAppId(const std::string appId, std::vector<UserSaveState>& out);
    static bool TryGetByAppIdAndVersionId(const std::string appId, const std::string versionId, UserSaveState& out);
    static bool PruneMissingPaths();
};
