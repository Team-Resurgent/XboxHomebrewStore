//=============================================================================
// JsonHelper.h - JSON helpers using parson
//=============================================================================

#pragma once

#include "parson.h"
#include "Main.h"

class JsonHelper
{
public:
    // ---- Reading (parse tree from json_parse_string) ----
    static JSON_Value* GetObjectMember( const JSON_Object* obj, const char* name );
    static std::string ToString( const JSON_Value* v );
    static std::vector<std::string> ToStringArray( const JSON_Value* v );
    static uint32_t ToUInt32( const JSON_Value* v );
    static int32_t ToInt( const JSON_Value* v );
    static bool ToBool( const JSON_Value* v );

    // ---- Writing (build tree, then json_serialize_to_string_pretty) ----
    static void FreeValue( JSON_Value* v );
    static JSON_Value* StringValue( const char* str, size_t len );
    static JSON_Value* NumberValue( int32_t n );
    static JSON_Value* BoolValue( bool isTrue );
    static bool ObjectAdd( JSON_Object* obj, const char* keyName, size_t keyLen, JSON_Value* value );
};
