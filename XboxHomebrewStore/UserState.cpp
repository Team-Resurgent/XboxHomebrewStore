////=============================================================================
//// UserState.cpp - User state persistence (load/save JSON, sync with store)
////=============================================================================
//
//#include "UserState.h"
//#include "JsonHelper.h"
//#include "parson.h"
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//
//UserState::UserState()
//{
//    version_ = "1.0";
//    last_updated_ = "2026-02-12";
//}
//
//void UserState::FromJson( const JSON_Object* rootObj )
//{
//    if( !rootObj ) return;
//    apps_.clear();
//    JSON_Value* vVer = JsonHelper::GetObjectMember( rootObj, "version" );
//    JSON_Value* vUpdated = JsonHelper::GetObjectMember( rootObj, "last_updated" );
//    if( vVer ) version_ = JsonHelper::ToString( vVer );
//    if( vUpdated ) last_updated_ = JsonHelper::ToString( vUpdated );
//
//    size_t n = json_object_get_count( rootObj );
//    for( size_t i = 0; i < n; i++ )
//    {
//        const char* key = json_object_get_name( rootObj, i );
//        if( !key ) continue;
//        if( strcmp( key, "version" ) == 0 ) continue;
//        if( strcmp( key, "last_updated" ) == 0 ) continue;
//
//        JSON_Value* elemVal = json_object_get_value_at( rootObj, i );
//        if( !elemVal ) continue;
//        JSON_Object* appObj = json_value_get_object( elemVal );
//        if( !appObj ) continue;
//
//        AppUserState appState;
//        appState.appId = key;
//        JSON_Value* vViewed = JsonHelper::GetObjectMember( appObj, "viewed" );
//        appState.viewed = JsonHelper::ToBool( vViewed );
//        JSON_Value* versVal = JsonHelper::GetObjectMember( appObj, "versions" );
//        JSON_Object* versObj = versVal ? json_value_get_object( versVal ) : NULL;
//        if( versObj )
//        {
//            size_t nv = json_object_get_count( versObj );
//            for( size_t j = 0; j < nv; j++ )
//            {
//                const char* verKey = json_object_get_name( versObj, j );
//                if( !verKey ) continue;
//                JSON_Value* veVal = json_object_get_value_at( versObj, j );
//                JSON_Object* verEntry = veVal ? json_value_get_object( veVal ) : NULL;
//                if( !verEntry ) continue;
//                VersionUserState vu;
//                vu.version = verKey;
//                JSON_Value* vs = JsonHelper::GetObjectMember( verEntry, "state" );
//                JSON_Value* vp = JsonHelper::GetObjectMember( verEntry, "path" );
//                vu.state = JsonHelper::ToInt( vs );
//                if( vp ) vu.install_path = JsonHelper::ToString( vp );
//                appState.versions.push_back( vu );
//            }
//        }
//        apps_.push_back( appState );
//    }
//}
//
//bool UserState::Load( const char* filename )
//{
//    OutputDebugString( "Loading user state...\n" );
//    FILE* f = fopen( filename, "rb" );
//    if( !f )
//    {
//        OutputDebugString( "No user state file found - this is normal on first run\n" );
//        return false;
//    }
//    fseek( f, 0, SEEK_END );
//    long fileSize = ftell( f );
//    fseek( f, 0, SEEK_SET );
//    char* fileData = new char[fileSize + 1];
//    if( !fileData )
//    {
//        OutputDebugString( "ERROR: Out of memory loading user state\n" );
//        fclose( f );
//        return false;
//    }
//    size_t readLen = fread( fileData, 1, (size_t)fileSize, f );
//    fileData[readLen] = '\0';
//    fclose( f );
//
//    JSON_Value* root = json_parse_string( fileData );
//    delete[] fileData;
//    if( !root )
//    {
//        OutputDebugString( "User state: JSON parse failed\n" );
//        return false;
//    }
//    JSON_Object* rootObj = json_value_get_object( root );
//    if( !rootObj )
//    {
//        json_value_free( root );
//        return false;
//    }
//    FromJson( rootObj );
//    json_value_free( root );
//    OutputDebugString( "User state applied\n" );
//    return true;
//}
//
//JSON_Value* UserState::ToJson() const
//{
//    JSON_Value* root = json_value_init_object();
//    if( !root ) return NULL;
//    JSON_Object* rootObj = json_value_get_object( root );
//
//    if( json_object_set_string( rootObj, "version", version_.c_str() ) != JSONSuccess ) goto fail;
//    if( json_object_set_string( rootObj, "last_updated", last_updated_.c_str() ) != JSONSuccess ) goto fail;
//
//    for( size_t a = 0; a < apps_.size(); a++ )
//    {
//        const AppUserState& app = apps_[a];
//        JSON_Value* appVal = json_value_init_object();
//        if( !appVal ) goto fail;
//        JSON_Object* appObj = json_value_get_object( appVal );
//        if( json_object_set_boolean( appObj, "viewed", app.viewed ? 1 : 0 ) != JSONSuccess ) { json_value_free( appVal ); goto fail; }
//
//        JSON_Value* versVal = json_value_init_object();
//        if( !versVal ) { json_value_free( appVal ); goto fail; }
//        JSON_Object* versObj = json_value_get_object( versVal );
//        for( size_t v = 0; v < app.versions.size(); v++ )
//        {
//            const VersionUserState& vu = app.versions[v];
//            JSON_Value* verEntryVal = json_value_init_object();
//            if( !verEntryVal ) { json_value_free( versVal ); json_value_free( appVal ); goto fail; }
//            JSON_Object* verEntry = json_value_get_object( verEntryVal );
//            if( json_object_set_number( verEntry, "state", (double)vu.state ) != JSONSuccess ) { json_value_free( verEntryVal ); json_value_free( versVal ); json_value_free( appVal ); goto fail; }
//            if( !vu.install_path.empty() )
//            {
//                if( json_object_set_string( verEntry, "path", vu.install_path.c_str() ) != JSONSuccess ) { json_value_free( verEntryVal ); json_value_free( versVal ); json_value_free( appVal ); goto fail; }
//            }
//            if( json_object_set_value( versObj, vu.version.c_str(), verEntryVal ) != JSONSuccess ) { json_value_free( verEntryVal ); json_value_free( versVal ); json_value_free( appVal ); goto fail; }
//        }
//        if( json_object_set_value( appObj, "versions", versVal ) != JSONSuccess ) { json_value_free( versVal ); json_value_free( appVal ); goto fail; }
//        if( json_object_set_value( rootObj, app.appId.c_str(), appVal ) != JSONSuccess ) { json_value_free( appVal ); goto fail; }
//    }
//    return root;
//fail:
//    json_value_free( root );
//    return NULL;
//}
//
//bool UserState::Save( const char* filename )
//{
//    OutputDebugString( "Saving user state...\n" );
//    JSON_Value* root = ToJson();
//    if( !root ) return false;
//    char* jsonStr = json_serialize_to_string_pretty( root );
//    json_value_free( root );
//    if( !jsonStr )
//    {
//        OutputDebugString( "Failed to serialize user state JSON\n" );
//        return false;
//    }
//    FILE* f = fopen( filename, "wb" );
//    if( !f )
//    {
//        json_free_serialized_string( jsonStr );
//        OutputDebugString( "Failed to create user state file\n" );
//        return false;
//    }
//    size_t len = strlen( jsonStr );
//    if( len > 0 ) { fwrite( jsonStr, 1, len, f ); }
//    fclose( f );
//    json_free_serialized_string( jsonStr );
//    OutputDebugString( "User state saved\n" );
//    return true;
//}
//
//void UserState::ApplyToStore( StoreItem* pItems, int nItemCount )
//{
//    /*for( size_t a = 0; a < apps_.size(); a++ )
//    {
//        const AppUserState& app = apps_[a];
//        for( int i = 0; i < nItemCount; i++ )
//        {
//            if( pItems[i].app.id != app.appId ) continue;
//            pItems[i].app.state = app.viewed ? STATE_NONE : pItems[i].app.state;
//            for( size_t v = 0; v < app.versions.size(); v++ )
//            {
//                const VersionUserState& vu = app.versions[v];
//                for( size_t vi = 0; vi < pItems[i].versions.size(); vi++ )
//                {
//                    if( pItems[i].versions[vi].version == vu.version )
//                    {
//                        pItems[i].versions[vi].state = vu.state;
//                        pItems[i].versions[vi].install_path = vu.install_path;
//                        break;
//                    }
//                }
//            }
//            break;
//        }
//    }*/
//}
//
//void UserState::UpdateFromStore( StoreItem* pItems, int nItemCount )
//{
//    /*version_ = "1.0";
//    last_updated_ = "2026-02-12";
//    apps_.clear();
//    for( int i = 0; i < nItemCount; i++ )
//    {
//        StoreItem* pItem = &pItems[i];
//        if( pItem->app.id.empty() ) continue;
//        bool viewed = pItem->app.state == STATE_NONE;
//        bool hasState = false;
//        for( size_t v = 0; v < pItem->versions.size(); v++ )
//        {
//            if( pItem->versions[v].state != STATE_NONE ) { hasState = true; break; }
//        }
//        if( !viewed && !hasState ) continue;
//        AppUserState appState;
//        appState.appId = pItem->app.id;
//        appState.viewed = viewed;
//        for( size_t v = 0; v < pItem->versions.size(); v++ )
//        {
//            const VersionItem& ver = pItem->versions[v];
//            VersionUserState vu;
//            vu.version = ver.version;
//            vu.state = ver.state;
//            vu.install_path = ver.install_path;
//            appState.versions.push_back( vu );
//        }
//        apps_.push_back( appState );
//    }*/
//}
//
//void UserState::MarkAppViewed( const char* appId )
//{
//    if( !appId ) return;
//    for( size_t i = 0; i < apps_.size(); i++ )
//    {
//        if( apps_[i].appId == appId )
//        {
//            apps_[i].viewed = true;
//            return;
//        }
//    }
//    AppUserState app;
//    app.appId = appId;
//    app.viewed = true;
//    apps_.push_back( app );
//}
//
//void UserState::SetVersionState( const char* appId, const char* version, int state, const char* installPath )
//{
//    if( !appId || !version ) return;
//    for( size_t i = 0; i < apps_.size(); i++ )
//    {
//        if( apps_[i].appId != appId ) continue;
//        for( size_t v = 0; v < apps_[i].versions.size(); v++ )
//        {
//            if( apps_[i].versions[v].version == version )
//            {
//                apps_[i].versions[v].state = state;
//                apps_[i].versions[v].install_path = installPath ? installPath : "";
//                return;
//            }
//        }
//        VersionUserState vu;
//        vu.version = version;
//        vu.state = state;
//        vu.install_path = installPath ? installPath : "";
//        apps_[i].versions.push_back( vu );
//        return;
//    }
//    AppUserState app;
//    app.appId = appId;
//    app.viewed = false;
//    VersionUserState vu;
//    vu.version = version;
//    vu.state = state;
//    vu.install_path = installPath ? installPath : "";
//    app.versions.push_back( vu );
//    apps_.push_back( app );
//}
