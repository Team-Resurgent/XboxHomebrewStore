//=============================================================================
// JsonHelper.cpp - JSON helpers implementation
//=============================================================================

#include "JsonHelper.h"
#include "String.h"
#include <stdlib.h>
#include <string.h>

struct json_value_s* JsonHelper::GetObjectMember( struct json_object_s* obj, const char* name )
{
    if( !obj || !name ) return NULL;
    size_t nameLen = strlen( name );
    struct json_object_element_s* elem = obj->start;
    while( elem )
    {
        if( elem->name && elem->name->string_size == nameLen &&
            memcmp( elem->name->string, name, nameLen ) == 0 )
        {
            return elem->value;
        }
        elem = elem->next;
    }
    return NULL;
}

std::string JsonHelper::ToString( struct json_value_s* v )
{
    if( !v ) { return ""; }
    struct json_string_s* s = json_value_as_string( v );
    if( !s || !s->string ) return "";
    return std::string( s->string, s->string_size );
}

uint32_t JsonHelper::ToUInt32( struct json_value_s* v )
{
    if( !v ) { return 0; }
    struct json_number_s* n = json_value_as_number( v );
    if( !n || !n->number ) { return 0; }
    return (uint32_t)strtoul( n->number, NULL, 10 );
}

int JsonHelper::ToInt( struct json_value_s* v )
{
    if( !v ) { return 0; }
    struct json_number_s* n = json_value_as_number( v );
    if( !n || !n->number ) { return 0; }
    return atoi( n->number );
}

bool JsonHelper::ToBool( struct json_value_s* v )
{
    return v && json_value_is_true( v );
}

void JsonHelper::FreeValue( struct json_value_s* v )
{
    if( !v ) { return; }
    if( v->type == json_type_object )
    {
        struct json_object_s* obj = (struct json_object_s*)v->payload;
        if( obj )
        {
            struct json_object_element_s* elem = obj->start;
            while( elem )
            {
                struct json_object_element_s* next = elem->next;
                if( elem->name ) { free( elem->name ); }
                if( elem->value ) { FreeValue( elem->value ); }
                free( elem );
                elem = next;
            }
            free( obj );
        }
    }
    else if( v->type == json_type_string )
    {
        if( v->payload ) { free( v->payload ); }
    }
    else if( v->type == json_type_number )
    {
        if( v->payload )
        {
            struct json_number_s* jn = (struct json_number_s*)v->payload;
            if( jn->number ) { free( (void*)jn->number ); }
            free( jn );
        }
    }
    free( v );
}

struct json_value_s* JsonHelper::StringValue( const char* str, size_t len )
{
    struct json_string_s* js = (struct json_string_s*)malloc( sizeof( struct json_string_s ) );
    if( !js ) { return NULL; }
    js->string = str;
    js->string_size = len;
    struct json_value_s* v = (struct json_value_s*)malloc( sizeof( struct json_value_s ) );
    if( !v ) { free( js ); return NULL; }
    v->type = json_type_string;
    v->payload = js;
    return v;
}

struct json_value_s* JsonHelper::NumberValue( int n )
{
    std::string str = String::Format( "%d", n );
    char* numStr = (char*)malloc( str.size() + 1 );
    if( !numStr ) { return NULL; }
    memcpy( numStr, str.c_str(), str.size() + 1 );
    struct json_number_s* jn = (struct json_number_s*)malloc( sizeof( struct json_number_s ) );
    if( !jn ) { free( numStr ); return NULL; }
    jn->number = numStr;
    jn->number_size = str.size();
    struct json_value_s* v = (struct json_value_s*)malloc( sizeof( struct json_value_s ) );
    if( !v ) { free( numStr ); free( jn ); return NULL; }
    v->type = json_type_number;
    v->payload = jn;
    return v;
}

struct json_value_s* JsonHelper::BoolValue( bool isTrue )
{
    struct json_value_s* v = (struct json_value_s*)malloc( sizeof( struct json_value_s ) );
    if( !v ) { return NULL; }
    v->type = isTrue ? json_type_true : json_type_false;
    v->payload = NULL;
    return v;
}

bool JsonHelper::ObjectAdd( struct json_object_s* obj, const char* keyName, size_t keyLen, struct json_value_s* value )
{
    struct json_string_s* name = (struct json_string_s*)malloc( sizeof( struct json_string_s ) );
    if( !name ) { return false; }
    name->string = keyName;
    name->string_size = keyLen;
    struct json_object_element_s* elem = (struct json_object_element_s*)malloc( sizeof( struct json_object_element_s ) );
    if( !elem ) { free( name ); return false; }
    elem->name = name;
    elem->value = value;
    elem->next = obj->start;
    obj->start = elem;
    obj->length++;
    return true;
}
