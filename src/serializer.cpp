
#include "serializer.h"

#include <cstring>

SerializerString::SerializerString( const char* str, bool static_str )
    : SerializerString( str, std::strlen( str ), static_str )
{}

bool operator==( const SerializerString lhs, const SerializerString rhs )
{
    return lhs.m_length == rhs.m_length && std::strcmp( lhs.m_string, rhs.m_string ) == 0;
}

bool operator!=( const SerializerString lhs, const SerializerString rhs )
{
    return !(lhs == rhs);
}

bool operator==( const SerializerString lhs, const char* rhs )
{
    return std::strcmp( lhs.m_string, rhs ) == 0;
}

bool operator!=( const SerializerString lhs, const char* rhs )
{
    return !( lhs == rhs );
}

void serialize( ISerializer& serializer, const SerializerString name, int& var )
{
    serializer.serialize( name, var );
}
void serialize( ISerializer& serializer, const SerializerString name, unsigned int& var )
{
    serializer.serialize( name, var );
}
void serialize( ISerializer& serializer, const SerializerString name, float& var )
{
    serializer.serialize( name, var );
}
void serialize( ISerializer& serializer, const SerializerString name, bool& var )
{
    serializer.serialize( name, var );
}
void serialize( ISerializer& serializer, const SerializerString name, const char*& str, unsigned& length )
{
    serializer.serialize( name, str, length );
}
void serialize( ISerializer& serializer, const SerializerString name, char& var )
{
    int temp = var;
    serializer.serialize( name, temp );
    var = static_cast< char >( temp );
}

void serialize( ISerializer& serializer, const SerializerString name, unsigned char& var )
{
    unsigned temp = var;
    serializer.serialize( name, temp );
    var = static_cast< unsigned char >( temp );
}

void serialize( ISerializer& serializer, const SerializerString name, short& var )
{
    int temp = var;
    serializer.serialize( name, temp );
    var = static_cast< short >( temp );
}

void serialize( ISerializer& serializer, const SerializerString name, unsigned short& var )
{
    unsigned temp = var;
    serializer.serialize( name, temp );
    var = static_cast< unsigned short >( temp );
}

void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< int >& arr )
{
    serializer.serialize( name, arr );
}
void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< unsigned int >& arr )
{
    serializer.serialize( name, arr );
}
void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< float >& arr )
{
    serializer.serialize( name, arr );
}
void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< bool >& arr )
{
    serializer.serialize( name, arr );
}
void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< SerializerString >& arr )
{
    serializer.serialize( name, arr );
}

void serialize( ISerializer& serializer, const SerializerString name, const int& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const unsigned int& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const float& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const bool& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const char* const& str, const unsigned& length )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto str2 = str;
    auto length2 = length;
    serializer.serialize( name, str2, length2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const char& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const unsigned char& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const short& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const unsigned short& var )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    auto var2 = var;
    serialize( serializer, name, var2 );
}
void serialize( ISerializer& serializer, const SerializerString name, const char* str )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    unsigned length = static_cast< unsigned >( std::strlen( str ) );
    serializer.serialize( name, str, length );
}

