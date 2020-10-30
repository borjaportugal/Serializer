
#include "serializer_std.h"

bool operator==( const SerializerString lhs, const std::string_view rhs )
{
    return lhs.m_length == rhs.length() && std::strcmp( lhs.m_string, rhs.data() ) == 0;
}
bool operator!=( const SerializerString lhs, const std::string_view rhs )
{
    return !( lhs == rhs );
}
bool operator==( const std::string_view lhs, const SerializerString rhs )
{
    return rhs == lhs;
}
bool operator!=( const std::string_view lhs, const SerializerString rhs )
{
    return rhs != lhs;
}

bool operator==( const SerializerString lhs, const std::string& rhs )
{
    return lhs.m_length == rhs.length() && std::strcmp( lhs.m_string, rhs.data() ) == 0;
}
bool operator!=( const SerializerString lhs, const std::string& rhs )
{
    return !( lhs == rhs );
}
bool operator==(const std::string& lhs, const SerializerString rhs)
{
    return rhs == lhs;
}
bool operator!=(const std::string& lhs, const SerializerString rhs)
{
    return rhs != lhs;
}


