//=============================================================================
// JsonHelper.cpp - JSON helpers implementation (parson)
//=============================================================================

#include "JsonHelper.h"
#include "String.h"
#include <stdlib.h>
#include <string.h>

JSON_Value* JsonHelper::GetObjectMember( const JSON_Object* obj, const char* name )
{
    if( !obj || !name ) return NULL;
    return json_object_get_value( obj, name );
}

std::string JsonHelper::ToString( const JSON_Value* v )
{
    if( !v ) return "";
    const char* s = json_value_get_string( v );
    if( !s ) return "";
    size_t len = json_value_get_string_len( v );
    return std::string( s, len );
}

uint32_t JsonHelper::ToUInt32( const JSON_Value* v )
{
    if( !v ) return 0;
    return (uint32_t)json_value_get_number( v );
}

int JsonHelper::ToInt( const JSON_Value* v )
{
    if( !v ) return 0;
    return (int)json_value_get_number( v );
}

bool JsonHelper::ToBool( const JSON_Value* v )
{
    return v && json_value_get_boolean( v ) != 0;
}

void JsonHelper::FreeValue( JSON_Value* v )
{
    if( v ) json_value_free( v );
}

JSON_Value* JsonHelper::StringValue( const char* str, size_t len )
{
    return json_value_init_string_with_len( str, len );
}

JSON_Value* JsonHelper::NumberValue( int n )
{
    return json_value_init_number( (double)n );
}

JSON_Value* JsonHelper::BoolValue( bool isTrue )
{
    return json_value_init_boolean( isTrue ? 1 : 0 );
}

bool JsonHelper::ObjectAdd( JSON_Object* obj, const char* keyName, size_t keyLen, JSON_Value* value )
{
    (void)keyLen;
    if( !obj || !keyName || !value ) return false;
    return json_object_set_value( obj, keyName, value ) == JSONSuccess;
}
