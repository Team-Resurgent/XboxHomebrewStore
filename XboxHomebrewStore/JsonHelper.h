//=============================================================================
// JsonHelper.h - JSON helpers using sheredom/json.h
//=============================================================================

#pragma once

#include "json.h"
#include <stdint.h>
#include <string>

class JsonHelper
{
public:
    // ---- Reading (parse tree from json_parse) ----
    static struct json_value_s* GetObjectMember( struct json_object_s* obj, const char* name );
    static std::string ToString( struct json_value_s* v );
    static uint32_t ToUInt32( struct json_value_s* v );
    static int ToInt( struct json_value_s* v );
    static bool ToBool( struct json_value_s* v );

    // ---- Writing (build tree, then json_write_minified) ----
    static void FreeValue( struct json_value_s* v );
    static struct json_value_s* StringValue( const char* str, size_t len );
    static struct json_value_s* NumberValue( int n );
    static struct json_value_s* BoolValue( bool isTrue );
    static bool ObjectAdd( struct json_object_s* obj, const char* keyName, size_t keyLen, struct json_value_s* value );
};
