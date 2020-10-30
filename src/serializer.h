
#pragma once

#include "serializer_config.h"

#ifndef SERIALIZER_API
#define SERIALIZER_API
#endif

#ifndef SERIALIZER_ASSERT
#include <assert.h>
#define SERIALIZER_ASSERT( b, str, ... ) assert( b && str #__VA_ARGS__ )
#endif

#include <cstddef>  // std::size_t

struct SERIALIZER_API SerializerString
{
    SerializerString()
        : SerializerString( "", 0, true )
    {}
    SerializerString( const char* str, bool static_str = false );
    constexpr SerializerString( const char* str, std::size_t length, bool static_str = false )
        : m_string{ str }
        , m_length{ static_cast< unsigned >( length ) }
        , m_static{ static_str }
    {}

    const char* m_string{ nullptr };
    bool m_static{ false };
    unsigned m_length{ 0 };

    // #study_borja Consider adding a hash to the string so that specializations of the serializer can benefit from it.
    //              This can be a big improvement in cases where the hash is computed at compile time.
};

// _ss stands for "Serializer String"
inline constexpr SerializerString operator ""_ss( const char* str, std::size_t length )
{
    constexpr bool static_str = true;
    return SerializerString{ str, length, static_str };
}

SERIALIZER_API bool operator==( const SerializerString lhs, const SerializerString rhs );
SERIALIZER_API bool operator!=( const SerializerString lhs, const SerializerString rhs );
SERIALIZER_API bool operator==( const SerializerString lhs, const char* rhs );
SERIALIZER_API bool operator!=( const SerializerString lhs, const char* rhs );

// #study_borja We cannot serialize array of arrays, we can only serialize an array of objects and inside each object an array:
//              Instead of: `[ [...], [...] ]`
//              We need to do: [ { array: [...] }, { array: [...] } ]

// Interface to serialize arrays of POD types.
// get_ methoths are used by Writers
// set_ methods are used by Readers
template < typename T >
struct SerializerArray
{
    // Interface used to serialize element by element
    // Should be implemented always.
    virtual unsigned get_size() const = 0;
    virtual void get_element( unsigned i, T& t ) const = 0;
    virtual void set_size( unsigned i ) const = 0;
    virtual void set_element( unsigned i, T t ) const = 0;
    
    // Optional interface used by serializers that store the data in a contiguous chunk (i.e. binary serializers could do this)
    // Doesn't need to be implemented, but in some cases will make serialization faster.
    virtual bool supports_get_set_all() const { return false; }
    virtual const T* get_all() const { SERIALIZER_ASSERT( !supports_get_set_all(), "Not supported!" ); SERIALIZER_ASSERT( false, "Not implemented!" ); return nullptr; };
    virtual void set_all( unsigned size, const T* const data ) const { SERIALIZER_ASSERT( !supports_get_set_all(), "Not supported!" ); SERIALIZER_ASSERT( false, "Not implemented!" ); }
};

class SERIALIZER_API ISerializer
{
public:
    using SerializeObjFn = void(*)( ISerializer&, void* );
    using IterateElementsFn = bool(*)( ISerializer&, const SerializerString name, void* );  // Return `true` to continue iterating
    using SerializeObjArrayFn = void(*)( ISerializer&, unsigned idx, void* user_data );

public:
    virtual ~ISerializer() = default;

    // Is a file reader? (i.e. JsonReader reads/loads a json file, BinaryReader reads/loads a binary file)
    virtual bool is_reader() const = 0;
    virtual bool has_member( const SerializerString name ) const = 0;
    
    virtual void serialize( const SerializerString name, int& var ) = 0;
    virtual void serialize( const SerializerString name, unsigned int& var ) = 0;
    virtual void serialize( const SerializerString name, float& var ) = 0;
    virtual void serialize( const SerializerString name, bool& var ) = 0;
    virtual void serialize( const SerializerString name, const char*& str, unsigned& length ) = 0;
    virtual void serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn ) = 0;
    virtual void iterate_elements( void* const user_data, const IterateElementsFn fn ) = 0;
    
    virtual void serialize( const SerializerString name, const SerializerArray< int >& var ) = 0;
    virtual void serialize( const SerializerString name, const SerializerArray< unsigned int >& var ) = 0;
    virtual void serialize( const SerializerString name, const SerializerArray< float >& var ) = 0;
    virtual void serialize( const SerializerString name, const SerializerArray< bool >& var ) = 0;
    virtual void serialize( const SerializerString name, const SerializerArray< SerializerString >& array ) = 0;
    
    virtual void write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn ) = 0;
    virtual unsigned read_object_array_size( const SerializerString name ) = 0;
    virtual void read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn ) = 0;

    // #study_borja Consider implementing `member_type( const SerializerString name ) const` so that the type of an specific variable can be requested before loading it.
    // #study_borja Consider returning bool from `serialize` functions to let the user know if the requested variable was loaded or not.
};

// Generic interface to load/save variables. 
// The global functions allow performing all calls to `serialize` in the same way, no matter if the passed object is constant or not.

// Interface that will load or save the variable depending on the ISerializer that is passed.
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, int& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, unsigned int& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, float& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, bool& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const char*& str, unsigned& length );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, char& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, unsigned char& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, short& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, unsigned short& var );

// `const` interface, these functions can only be called with Writers.
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< int >& arr );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< unsigned int >& arr );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< float >& arr );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< bool >& arr );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const SerializerArray< SerializerString >& arr );

SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const int& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const unsigned int& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const float& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const bool& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const char* const& str, const unsigned& length );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const char& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const unsigned char& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const short& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const unsigned short& var );
SERIALIZER_API void serialize( ISerializer& serializer, const SerializerString name, const char* str );

// Helper functions to make calling the functions in `ISerializer` simpler using lambdas or function objects.

// `Fn` must be a callable object with signature `void fn( ISerializer& s )`
template < typename Fn >
inline void serialize_object( ISerializer& serializer, const SerializerString name, Fn fn )
{
    serializer.serialize( name, reinterpret_cast< void* >( &fn ), []( ISerializer& serializer, void* const user_data )
    {
        Fn& fn = *static_cast< Fn* >( user_data );
        fn( serializer );
    } );
}

// `Fn` must be a callable object with signature `bool fn( ISerializer& s, const SerializerString name )`
template < typename Fn >
inline void iterate_elements( ISerializer& serializer, Fn fn )
{
    serializer.iterate_elements( reinterpret_cast< void* >( &fn ), []( ISerializer& serializer, const SerializerString name, void* user_data ) -> bool
    {
        Fn& fn = *static_cast< Fn* >( user_data );
        return fn( serializer, name );
    } );
}

// `Fn` must be a callable object with signature `void fn( ISerializer& s, unsigned idx )`
template < typename Fn >
inline void serializer_write_object_array( ISerializer& s, const SerializerString name, unsigned array_size, Fn fn )
{
    SERIALIZER_ASSERT( !s.is_reader(), "Only writers are supported in this function." );
    s.write_object_array( name, array_size, &fn, []( ISerializer& s, unsigned idx, void* user_data )
    {
        Fn& fn = *static_cast< Fn* >( user_data );
        fn( s, idx );
    } );
}

// `Fn` must be a callable object with signature `void fn( ISerializer& s, unsigned idx )`
template < typename Fn >
inline void serializer_read_object_array( ISerializer& s, const SerializerString name, Fn fn )
{
    SERIALIZER_ASSERT( s.is_reader(), "Only readers are supported in this function." );
    s.read_object_array( name, &fn, []( ISerializer& s, unsigned idx, void* user_data )
    {
        Fn& fn = *static_cast< Fn* >( user_data );
        fn( s, idx );
    } );
}

