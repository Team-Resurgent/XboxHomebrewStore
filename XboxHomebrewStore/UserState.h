#pragma once

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
};
