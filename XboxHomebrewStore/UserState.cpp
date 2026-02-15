//=============================================================================
// UserState.cpp - User state persistence (load/save JSON, sync with store)
//=============================================================================

#include "Store.h"
#include "JsonHelper.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

UserState::UserState()
{
    version_ = "1.0";
    last_updated_ = "2026-02-12";
}

void UserState::FromJson( struct json_object_s* rootObj )
{
    if( !rootObj ) return;
    apps_.clear();
    struct json_value_s* vVer = JsonHelper::GetObjectMember( rootObj, "version" );
    struct json_value_s* vUpdated = JsonHelper::GetObjectMember( rootObj, "last_updated" );
    if( vVer ) version_ = JsonHelper::ToString( vVer );
    if( vUpdated ) last_updated_ = JsonHelper::ToString( vUpdated );

    for( struct json_object_element_s* elem = rootObj->start; elem != NULL; elem = elem->next )
    {
        if( !elem->name || !elem->value ) continue;
        const char* key = elem->name->string;
        size_t keyLen = elem->name->string_size;
        if( keyLen == 7 && memcmp( key, "version", 7 ) == 0 ) continue;
        if( keyLen == 12 && memcmp( key, "last_updated", 12 ) == 0 ) continue;

        AppUserState appState;
        appState.appId.assign( key, keyLen );
        struct json_object_s* appObj = json_value_as_object( elem->value );
        if( !appObj ) continue;
        struct json_value_s* vViewed = JsonHelper::GetObjectMember( appObj, "viewed" );
        appState.viewed = JsonHelper::ToBool( vViewed );
        struct json_value_s* versVal = JsonHelper::GetObjectMember( appObj, "versions" );
        struct json_object_s* versObj = versVal ? json_value_as_object( versVal ) : NULL;
        if( versObj )
        {
            for( struct json_object_element_s* ve = versObj->start; ve != NULL; ve = ve->next )
            {
                if( !ve->name || !ve->value ) continue;
                VersionUserState vu;
                vu.version.assign( ve->name->string, ve->name->string_size );
                struct json_object_s* verEntry = json_value_as_object( ve->value );
                if( verEntry )
                {
                    struct json_value_s* vs = JsonHelper::GetObjectMember( verEntry, "state" );
                    struct json_value_s* vp = JsonHelper::GetObjectMember( verEntry, "path" );
                    vu.state = JsonHelper::ToInt( vs );
                    if( vp ) vu.install_path = JsonHelper::ToString( vp );
                }
                appState.versions.push_back( vu );
            }
        }
        apps_.push_back( appState );
    }
}

bool UserState::Load( const char* filename )
{
    OutputDebugString( "Loading user state...\n" );
    FILE* f = fopen( filename, "rb" );
    if( !f )
    {
        OutputDebugString( "No user state file found - this is normal on first run\n" );
        return false;
    }
    fseek( f, 0, SEEK_END );
    long fileSize = ftell( f );
    fseek( f, 0, SEEK_SET );
    char* fileData = new char[fileSize + 1];
    if( !fileData )
    {
        OutputDebugString( "ERROR: Out of memory loading user state\n" );
        fclose( f );
        return false;
    }
    size_t readLen = fread( fileData, 1, (size_t)fileSize, f );
    fileData[readLen] = '\0';
    fclose( f );

    struct json_value_s* root = json_parse( fileData, readLen );
    delete[] fileData;
    if( !root )
    {
        OutputDebugString( "User state: JSON parse failed\n" );
        return false;
    }
    struct json_object_s* rootObj = json_value_as_object( root );
    if( !rootObj )
    {
        JsonHelper::FreeValue( root );
        return false;
    }
    FromJson( rootObj );
    JsonHelper::FreeValue( root );
    OutputDebugString( "User state applied\n" );
    return true;
}

struct json_value_s* UserState::ToJson() const
{
    struct json_object_s* rootObj = (struct json_object_s*)malloc( sizeof(struct json_object_s) );
    if( !rootObj ) return NULL;
    rootObj->length = 0;
    rootObj->start = NULL;
    struct json_value_s* root = (struct json_value_s*)malloc( sizeof(struct json_value_s) );
    if( !root ) { free( rootObj ); return NULL; }
    root->type = json_type_object;
    root->payload = rootObj;

    if( !JsonHelper::ObjectAdd( rootObj, "version", 7, JsonHelper::StringValue( version_.c_str(), version_.size() ) ) ) goto fail;
    if( !JsonHelper::ObjectAdd( rootObj, "last_updated", 12, JsonHelper::StringValue( last_updated_.c_str(), last_updated_.size() ) ) ) goto fail;

    for( size_t a = 0; a < apps_.size(); a++ )
    {
        const AppUserState& app = apps_[a];
        struct json_object_s* appObj = (struct json_object_s*)malloc( sizeof(struct json_object_s) );
        if( !appObj ) goto fail;
        appObj->length = 0;
        appObj->start = NULL;
        struct json_value_s* viewedVal = JsonHelper::BoolValue( app.viewed );
        if( !viewedVal ) { free( appObj ); goto fail; }
        if( !JsonHelper::ObjectAdd( appObj, "viewed", 6, viewedVal ) ) { JsonHelper::FreeValue( viewedVal ); free( appObj ); goto fail; }
        struct json_object_s* versObj = (struct json_object_s*)malloc( sizeof(struct json_object_s) );
        if( !versObj ) { free( appObj ); goto fail; }
        versObj->length = 0;
        versObj->start = NULL;
        for( size_t v = 0; v < app.versions.size(); v++ )
        {
            const VersionUserState& vu = app.versions[v];
            struct json_object_s* verEntry = (struct json_object_s*)malloc( sizeof(struct json_object_s) );
            if( !verEntry ) { free( versObj ); free( appObj ); goto fail; }
            verEntry->length = 0;
            verEntry->start = NULL;
            struct json_value_s* stateVal = JsonHelper::NumberValue( vu.state );
            if( !stateVal ) { free( verEntry ); free( versObj ); free( appObj ); goto fail; }
            if( !JsonHelper::ObjectAdd( verEntry, "state", 5, stateVal ) ) { JsonHelper::FreeValue( stateVal ); free( verEntry ); free( versObj ); free( appObj ); goto fail; }
            if( !vu.install_path.empty() )
            {
                struct json_value_s* pathVal = JsonHelper::StringValue( vu.install_path.c_str(), vu.install_path.size() );
                if( !pathVal ) { free( verEntry ); free( versObj ); free( appObj ); goto fail; }
                if( !JsonHelper::ObjectAdd( verEntry, "path", 4, pathVal ) ) { JsonHelper::FreeValue( pathVal ); free( verEntry ); free( versObj ); free( appObj ); goto fail; }
            }
            struct json_value_s* verEntryVal = (struct json_value_s*)malloc( sizeof(struct json_value_s) );
            if( !verEntryVal ) { free( verEntry ); free( versObj ); free( appObj ); goto fail; }
            verEntryVal->type = json_type_object;
            verEntryVal->payload = verEntry;
            if( !JsonHelper::ObjectAdd( versObj, vu.version.c_str(), vu.version.size(), verEntryVal ) ) { JsonHelper::FreeValue( verEntryVal ); free( versObj ); free( appObj ); goto fail; }
        }
        struct json_value_s* versVal = (struct json_value_s*)malloc( sizeof(struct json_value_s) );
        if( !versVal ) { free( versObj ); free( appObj ); goto fail; }
        versVal->type = json_type_object;
        versVal->payload = versObj;
        if( !JsonHelper::ObjectAdd( appObj, "versions", 8, versVal ) ) { JsonHelper::FreeValue( versVal ); free( appObj ); goto fail; }
        struct json_value_s* appVal = (struct json_value_s*)malloc( sizeof(struct json_value_s) );
        if( !appVal ) { free( appObj ); goto fail; }
        appVal->type = json_type_object;
        appVal->payload = appObj;
        if( !JsonHelper::ObjectAdd( rootObj, app.appId.c_str(), app.appId.size(), appVal ) ) { JsonHelper::FreeValue( appVal ); goto fail; }
    }
    return root;
fail:
    JsonHelper::FreeValue( root );
    return NULL;
}

bool UserState::Save( const char* filename )
{
    OutputDebugString( "Saving user state...\n" );
    struct json_value_s* root = ToJson();
    if( !root ) return false;
    size_t outSize = 0;
    void* jsonStr = json_write_minified( root, &outSize );
    JsonHelper::FreeValue( root );
    if( !jsonStr ) return false;
    FILE* f = fopen( filename, "wb" );
    if( !f )
    {
        free( jsonStr );
        OutputDebugString( "Failed to create user state file\n" );
        return false;
    }
    if( outSize > 0 ) {
        fwrite( jsonStr, 1, outSize, f );
    }
    fclose( f );
    free( jsonStr );
    OutputDebugString( "User state saved\n" );
    return true;
}

void UserState::ApplyToStore( StoreItem* pItems, int nItemCount )
{
    for( size_t a = 0; a < apps_.size(); a++ )
    {
        const AppUserState& app = apps_[a];
        for( int i = 0; i < nItemCount; i++ )
        {
            if( pItems[i].app.id != app.appId ) continue;
            pItems[i].app.isNew = !app.viewed;
            for( size_t v = 0; v < app.versions.size(); v++ )
            {
                const VersionUserState& vu = app.versions[v];
                for( size_t vi = 0; vi < pItems[i].versions.size(); vi++ )
                {
                    if( pItems[i].versions[vi].version == vu.version )
                    {
                        pItems[i].versions[vi].state = vu.state;
                        pItems[i].versions[vi].install_path = vu.install_path;
                        break;
                    }
                }
            }
            break;
        }
    }
}

void UserState::UpdateFromStore( StoreItem* pItems, int nItemCount )
{
    version_ = "1.0";
    last_updated_ = "2026-02-12";
    apps_.clear();
    for( int i = 0; i < nItemCount; i++ )
    {
        StoreItem* pItem = &pItems[i];
        if( pItem->app.id.empty() ) continue;
        bool viewed = !pItem->app.isNew;
        bool hasState = false;
        for( size_t v = 0; v < pItem->versions.size(); v++ )
        {
            if( pItem->versions[v].state != 0 ) { hasState = true; break; }
        }
        if( !viewed && !hasState ) continue;
        AppUserState appState;
        appState.appId = pItem->app.id;
        appState.viewed = viewed;
        for( size_t v = 0; v < pItem->versions.size(); v++ )
        {
            const VersionItem& ver = pItem->versions[v];
            VersionUserState vu;
            vu.version = ver.version;
            vu.state = ver.state;
            vu.install_path = ver.install_path;
            appState.versions.push_back( vu );
        }
        apps_.push_back( appState );
    }
}

void UserState::MarkAppViewed( const char* appId )
{
    if( !appId ) return;
    for( size_t i = 0; i < apps_.size(); i++ )
    {
        if( apps_[i].appId == appId )
        {
            apps_[i].viewed = true;
            return;
        }
    }
    AppUserState app;
    app.appId = appId;
    app.viewed = true;
    apps_.push_back( app );
}

void UserState::SetVersionState( const char* appId, const char* version, int state, const char* installPath )
{
    if( !appId || !version ) return;
    for( size_t i = 0; i < apps_.size(); i++ )
    {
        if( apps_[i].appId != appId ) continue;
        for( size_t v = 0; v < apps_[i].versions.size(); v++ )
        {
            if( apps_[i].versions[v].version == version )
            {
                apps_[i].versions[v].state = state;
                apps_[i].versions[v].install_path = installPath ? installPath : "";
                return;
            }
        }
        VersionUserState vu;
        vu.version = version;
        vu.state = state;
        vu.install_path = installPath ? installPath : "";
        apps_[i].versions.push_back( vu );
        return;
    }
    AppUserState app;
    app.appId = appId;
    app.viewed = false;
    VersionUserState vu;
    vu.version = version;
    vu.state = state;
    vu.install_path = installPath ? installPath : "";
    app.versions.push_back( vu );
    apps_.push_back( app );
}
