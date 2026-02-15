//=============================================================================
// UserState.h - User state persistence (load/save JSON, sync with store)
//=============================================================================

#pragma once

#include <string>
#include <vector>

struct StoreItem;

// User state persistence model (array of app states)
struct VersionUserState
{
    std::string version;
    int state;
    std::string install_path;
};

struct AppUserState
{
    std::string appId;
    bool viewed;
    std::vector<VersionUserState> versions;
};

struct json_value_s;
struct json_object_s;

// Holds current user state; Load/Save to JSON, ApplyToStore/UpdateFromStore for sync with catalog
class UserState
{
public:
    UserState();

    bool Load( const char* filename );
    bool Save( const char* filename );

    void ApplyToStore( StoreItem* pItems, int nItemCount );
    void UpdateFromStore( StoreItem* pItems, int nItemCount );

    void MarkAppViewed( const char* appId );
    void SetVersionState( const char* appId, const char* version, int state, const char* installPath = "" );

private:
    std::string version_;
    std::string last_updated_;
    std::vector<AppUserState> apps_;

    struct json_value_s* ToJson() const;
    void FromJson( struct json_object_s* rootObj );
};
