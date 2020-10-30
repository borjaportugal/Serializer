// This file extends the serializer interface with support for some of the stl types.
// Users can split the contents of this file in separate files to avoid some #includes of the stl where they are not needed.

#pragma once

#include "serializer.h"

#include <string_view>
SERIALIZER_API bool operator==( const SerializerString lhs, const std::string_view rhs );
SERIALIZER_API bool operator!=( const SerializerString lhs, const std::string_view rhs );
SERIALIZER_API bool operator==( const std::string_view lhs, const SerializerString rhs );
SERIALIZER_API bool operator!=( const std::string_view lhs, const SerializerString rhs );

#include <string>
SERIALIZER_API bool operator==(const SerializerString lhs, const std::string& rhs);
SERIALIZER_API bool operator!=(const SerializerString lhs, const std::string& rhs);
SERIALIZER_API bool operator==(const std::string& lhs, const SerializerString rhs );
SERIALIZER_API bool operator!=(const std::string& lhs, const SerializerString rhs );

inline std::string to_std_string( const SerializerString ss )
{
    return std::string{ ss.m_string, ss.m_length };
}

inline SerializerString to_serializer_string( const std::string& str )
{
    return SerializerString{ str.c_str(), str.length() };
}

template < typename Alloc >
void serialize( ISerializer& serializer, const SerializerString name, const std::basic_string< char, std::char_traits< char >, Alloc >& str )
{
    SERIALIZER_ASSERT( !serializer.is_reader(), "This function only supports 'Writer' serializers. Cannot read data from the serializer into the variable because is constant." );
    serialize( serializer, name, str.c_str(), str.length() );
}
template < typename Alloc >
void serialize( ISerializer& serializer, const SerializerString name, std::basic_string< char, std::char_traits< char >, Alloc >& str )
{
    if( serializer.is_reader() )
    {
        const char* temp_str = nullptr;
        unsigned temp_length = 0;
        serializer.serialize( name, temp_str, temp_length );
        
        if( temp_str )
            str = std::string{ temp_str, temp_length };
    }
    else
    {
        const char* temp_str = str.c_str();
        unsigned temp_length = str.length();
        serializer.serialize( name, temp_str, temp_length );
    }
    
}

#include <vector>
template < typename T, typename Alloc >
class SerializerVector : public SerializerArray< T >
{
public:
    explicit SerializerVector( const std::vector< T, Alloc >& gettor, std::vector< T, Alloc >* settor ) : m_gettor{ gettor }, m_settor{ settor } {}

    unsigned get_size() const override
    {
        return m_gettor.size();
    }
    void get_element( unsigned i, T& t ) const override
    {
        t = m_gettor[i];
    }
    
    void set_size( unsigned i ) const override
    {
        SERIALIZER_ASSERT( m_settor, "Cannot write into this array!" );
        m_settor->resize( i );
    }
    void set_element( unsigned i, T t ) const override
    {
        SERIALIZER_ASSERT( m_settor, "Cannot write into this array!" );
        m_settor->operator[](i) = t;
    }

    bool supports_get_set_all() const override { return true; }
    void set_all( unsigned size, const T* const data ) const override
    {
        SERIALIZER_ASSERT( m_settor, "Cannot write into this array!" );
        m_settor->resize( size );

        // We assume that T is a pod type
        std::memcpy( m_settor->data(), data, size * sizeof( T ) );
    }
    const T* get_all() const override
    {
        return m_gettor.data();
    }

    const std::vector< T, Alloc >& m_gettor;
    std::vector< T, Alloc >* const m_settor{ nullptr };
};

// `std::vector< bool >` doesn't have `data()` member function, we cannot use the `get_/set_all` interface
template < typename Alloc >
class SERIALIZER_API SerializerVector< bool, Alloc > : public SerializerArray< bool >
{
public:
    SerializerVector( const std::vector< bool >& gettor, std::vector< bool >* const settor ) : m_gettor{ gettor }, m_settor{ settor } {}

    unsigned get_size() const override
    {
        return m_gettor.size();
    }
    void get_element( unsigned i, bool& t ) const override
    {
        t = m_gettor[i];
    }
    
    void set_size( unsigned i ) const override
    {
        m_settor->resize( i );
    }
    void set_element( unsigned i, bool t ) const override
    {
        m_settor->operator[](i) = t;
    }

    const std::vector< bool >& m_gettor;
    std::vector< bool >* const m_settor{ nullptr };
};

template < typename T, typename Alloc >
void serialize( ISerializer& s, const SerializerString name, std::vector< T, Alloc >& v )
{
    s.serialize( name, SerializerVector< T, Alloc >{ v, &v } );
}
template < typename T, typename Alloc >
void serialize( ISerializer& s, const SerializerString name, const std::vector< T, Alloc >& v )
{
    SERIALIZER_ASSERT( !s.is_reader(), "" );
    s.serialize( name, SerializerVector< T, Alloc >{ v, nullptr } );
}


template < typename Alloc >
class SERIALIZER_API SerializerVector< SerializerString, Alloc > : public SerializerArray< SerializerString >
{
public:
    SerializerVector( const std::vector< std::string, Alloc >& gettor, std::vector< std::string, Alloc >* const settor ) : m_gettor{ gettor }, m_settor{ settor } {}

    unsigned get_size() const override
    {
        return m_gettor.size();
    }
    void get_element( unsigned i, SerializerString& t ) const override
    {
        SERIALIZER_ASSERT( i < m_gettor.size(), "" );
        t = to_serializer_string( m_gettor[i] );
    }
    
    void set_size( unsigned i ) const override
    {
        m_settor->resize( i );
    }
    void set_element( unsigned i, SerializerString t ) const override
    {
        SERIALIZER_ASSERT( i < m_settor->size(), "" );
        m_settor->operator[](i) = to_std_string( t );
    }

    const std::vector< std::string, Alloc >& m_gettor;
    std::vector< std::string, Alloc >* const m_settor{ nullptr };
};

template < typename Alloc >
inline void serialize( ISerializer& s, const SerializerString name, std::vector< std::string, Alloc >& v )
{
    s.serialize( name, SerializerVector< SerializerString, Alloc >{ v, &v } );
}

template < typename Alloc >
inline void serialize( ISerializer& s, const SerializerString name, const std::vector< std::string, Alloc >& v )
{
    SERIALIZER_ASSERT( !s.is_reader(), "" );
    s.serialize( name, SerializerVector< SerializerString, Alloc >{ v, nullptr } );
}
