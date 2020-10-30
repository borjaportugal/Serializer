
#include "json_serializer.h"

#include "asielorz-json/value.hh"

namespace
{
    inline json::value& access_child( const SerializerString name, json::value& value )
    {
        if( !value.is_object() )
            value = json::object{};

        json::object& object = value.as_object();

        if( name.m_static )
            return object[ json::static_string{ name.m_string, name.m_length } ];
        
        return object[ name.m_string ];
    }
    
    inline const json::value& access_child( const SerializerString name, const json::value& value )
    {
        if( !value.is_object() )
            return json::value::null;

        if( name.m_static )
            return value.as_object()[ json::static_string{ name.m_string, name.m_length } ];
        
        return value.as_object()[ name.m_string ];
    }

    SerializerString to_ss( const json::string_key& str )
    {
        return SerializerString{ str.c_str(), str.length(), str.is_view() };
    }

    template < typename T >
    void write_array( json::value& value, const SerializerArray< T >& array, const T default_value )
    {
        json::array json_array;
        const unsigned array_size = array.get_size();
        json_array.resize( array_size );
        for( unsigned i = 0; i < array_size; ++i )
        {
            T var = default_value;
            array.get_element( i, var );
            json_array[i] = var;
        }
        value = std::move( json_array );
    }

    template < typename T, T (json::value::*FN)() const >
    void read_array( const json::value& value, const SerializerArray< T >& array )
    {
        if( value.is_null() )
            return;

        if( value.is_array() )
        {
            const json::array& json_array = value.as_array();
            const unsigned size = json_array.size();
            array.set_size( size );
            for( unsigned i = 0; i < size; ++i )
                array.set_element( i, (json_array[i].*FN)() );
        }
        else
        {
            array.set_size( 1 );
            array.set_element( 0, (value.*FN)() );
        }
    }
}

bool JsonWriter::is_reader() const
{
    return false;
}
bool JsonWriter::has_member(const SerializerString name) const
{
    return !access_child(name, m_value).is_null();
}

void JsonWriter::serialize( const SerializerString name, int& var )
{
    access_child( name, m_value ) = var;
}

void JsonWriter::serialize( const SerializerString name, unsigned int& var )
{
    access_child( name, m_value ) = var;
}

void JsonWriter::serialize( const SerializerString name, float& var )
{
    access_child( name, m_value ) = var;
}

void JsonWriter::serialize( const SerializerString name, bool& var )
{
    access_child( name, m_value ) = var;
}

void JsonWriter::serialize( const SerializerString name, const char*& str, unsigned& length )
{
    access_child( name, m_value ) = std::string{ str };
}

void JsonWriter::serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn )
{
    json::value sub_object;
    JsonWriter sub_object_writer{ sub_object };
    fn( sub_object_writer, user_data );

    if( !sub_object.is_null() )
        access_child( name, m_value ) = std::move( sub_object );
}

void JsonWriter::iterate_elements( void* const user_data, const IterateElementsFn fn )
{
    if( !m_value.is_object() )
        return;
    
    for( auto it = m_value.as_object().begin(), end = m_value.as_object().end(); it != end; ++it )
    {
        if( !fn( *this, to_ss( it->first ), user_data ) )
        {
            break;
        }
    }
}

void JsonWriter::serialize( const SerializerString name, const SerializerArray< int >& array )
{
    write_array( access_child( name, m_value ), array, 0 );
}

void JsonWriter::serialize( const SerializerString name, const SerializerArray< unsigned int >& array )
{
    write_array( access_child( name, m_value ), array, 0u );
}

void JsonWriter::serialize( const SerializerString name, const SerializerArray< float >& array )
{
    write_array( access_child( name, m_value ), array, 0.f );
}

void JsonWriter::serialize( const SerializerString name, const SerializerArray< bool >& array )
{
    write_array( access_child( name, m_value ), array, false );
}

void JsonWriter::serialize( const SerializerString name, const SerializerArray< SerializerString >& array )
{
    json::array json_array;
    const unsigned array_size = array.get_size();
    json_array.resize(array_size);
    for( unsigned i = 0; i < array_size; ++i )
    {
        SerializerString str;
        array.get_element( i, str );
        json_array[i] = std::string{ str.m_string, str.m_length };
    }
    access_child( name, m_value ) = std::move( json_array );
}

void JsonWriter::write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn )
{
    json::array json_array;
    json_array.resize( element_num );
    for( unsigned i = 0; i < element_num; ++i )
    {
        JsonWriter writer{ json_array[i] };
        fn( writer, i, user_data );
    }
    access_child( name, m_value ) = std::move( json_array );
}
unsigned JsonWriter::read_object_array_size( const SerializerString name )
{
    SERIALIZER_ASSERT( false, "Not supported" );
    return 0;
}
void JsonWriter::read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn )
{
    SERIALIZER_ASSERT( false, "Not supported" );
}



bool JsonReader::is_reader() const
{
    return true;
}
bool JsonReader::has_member(const SerializerString name) const
{
    return !access_child(name, m_value).is_null();
}

void JsonReader::serialize( const SerializerString name, int& var )
{
    const json::value& value = access_child( name, m_value );
    if( value.is_numeric() )
        var = value.as_int();
    else if( value.is_bool() )
        var = value.as_bool() ? 1 : 0;
}

void JsonReader::serialize( const SerializerString name, unsigned int& var )
{
    const json::value& value = access_child( name, m_value );
    if( value.is_numeric() )
        var = value.as_uint();
    else if( value.is_bool() )
        var = value.as_bool() ? 1 : 0;
}

void JsonReader::serialize( const SerializerString name, float& var )
{
    const json::value& value = access_child( name, m_value );
    if( value.is_numeric() )
        var = value.as_float();
    else if( value.is_bool() )
        var = value.as_bool() ? 1.f : 0;
}

void JsonReader::serialize( const SerializerString name, bool& var )
{
    const json::value& value = access_child( name, m_value );
    if( value.is_bool() )
        var = value.as_bool();
    else if( value.is_numeric() )
        var = value.as_float() > 0;
}

void JsonReader::serialize( const SerializerString name, const char*& str, unsigned& length )
{
    const json::value& value = access_child( name, m_value );
    if( !value.is_null() )
    {
        str = value.as_c_string();    // We cannot use `asString()` here because that would create a local string (we need the pointer to persist after the call to this function)
        length = static_cast< unsigned >( std::strlen( str ) );
    }
}

void JsonReader::serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn )
{
    const json::value& value = access_child( name, m_value );
    if( !value.is_null() )
    {
        JsonReader sub_object_writer{ value };
        fn( sub_object_writer, user_data );
    }
}

void JsonReader::iterate_elements( void* const user_data, const IterateElementsFn fn )
{
    if( !m_value.is_object() )
        return;
    
    
    for( auto it = m_value.as_object().begin(), end = m_value.as_object().end(); it != end; ++it )
    {
        if( !fn( *this, to_ss( it->first ), user_data ) )
            break;
    }
}

void JsonReader::serialize( const SerializerString name, const SerializerArray< int >& array )
{
    read_array< int, &json::value::as_int >( access_child( name, m_value ), array );
}

void JsonReader::serialize( const SerializerString name, const SerializerArray< unsigned int >& array )
{
    read_array< unsigned, &json::value::as_uint >( access_child( name, m_value ), array );
}

void JsonReader::serialize( const SerializerString name, const SerializerArray< float >& array )
{
    read_array< float, &json::value::as_float >( access_child( name, m_value ), array );
}

void JsonReader::serialize( const SerializerString name, const SerializerArray< bool >& array )
{
    read_array< bool, &json::value::as_bool >( access_child( name, m_value ), array );
}

void JsonReader::serialize( const SerializerString name, const SerializerArray< SerializerString >& array )
{
    const json::value& value = access_child( name, m_value );
    if( value.is_array() )
    {
        const json::array& json_array = value.as_array();
        const unsigned size = json_array.size();
        array.set_size( size );
        for( unsigned i = 0; i < size; ++i )
        {
            if( !json_array[i].is_string() )
            {
                // #study_borja Consider asserting? Reporting some error message with a new SERIALIZER_WARNING macro?
                continue;
            }

            const std::string& str = json_array[i].as_c_string();
            const SerializerString ss{ str.c_str(), str.length() };
            array.set_element( i, ss );
        }
    }
    else if( value.is_string() )
    {
        const std::string& str = value.as_c_string();
        const SerializerString ss{ str.c_str(), str.length() };
        array.set_size( 1 );
        array.set_element( 0, ss );
    }
}

void JsonReader::write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn )
{
    SERIALIZER_ASSERT( false, "Not supported" );
}
unsigned JsonReader::read_object_array_size( const SerializerString name )
{
    const json::value& value = access_child( name, m_value );
    if( value.is_null() )
        return 0;
    return value.as_array().size();
}
void JsonReader::read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn )
{
    const json::value& value = access_child( name, m_value );
    if( !value.is_array() )
        return;

    const json::array& array = value.as_array();
    const unsigned size = array.size();
    for( unsigned i = 0; i < size; ++i )
    {
        const json::value& i_value = array[i];
        if( !i_value.is_null() )
        {
            JsonReader reader{ i_value };
            fn( reader, i, user_data );
        }
    } 
}

namespace 
{
    template < typename T, T(json::value::* FN )() const >
    struct JsonValueArray : SerializerArray< T >
    {
        explicit JsonValueArray( const json::array& array ) : m_array_value{ array } {}
        
        unsigned get_size() const override
        {
            return m_array_value.size();
        }
        void get_element( unsigned i, T& t ) const override
        {
            if( !m_array_value[i].is_null() )
                t = (m_array_value[i].*FN)();
        }
        
        void set_size( unsigned i ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        void set_element( unsigned i, T t ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        
        const json::array& m_array_value;
    };

    struct JsonStringArray : SerializerArray< SerializerString >
    {
        explicit JsonStringArray(const json::array& array) : m_array_value{ array } {}

        unsigned get_size() const override
        {
            return m_array_value.size();
        }
        void get_element(unsigned i, SerializerString& t) const override
        {
            if (!m_array_value[i].is_null())
                t = SerializerString{ m_array_value[i].as_c_string() };
        }

        void set_size(unsigned i) const override
        {
            SERIALIZER_ASSERT(false, "Shouldn't be called!");
        }
        void set_element(unsigned i, SerializerString t) const override
        {
            SERIALIZER_ASSERT(false, "Shouldn't be called!");
        }

        const json::array& m_array_value;
    };
}

void json_to_other( const json::value& json_value, const SerializerString member_name, ISerializer& writer )
{
    SERIALIZER_ASSERT( !writer.is_reader(), "Expecting a writer." );

    if( json_value.is_object() )
    {
        serialize_object( writer, member_name, [&json_value]( ISerializer& writer )
        {
            for( auto it = json_value.as_object().begin(), end = json_value.as_object().end(); it != end; ++it )
            {
                json_to_other( it->second, it->first.c_str(), writer );
            }
        } );
    }
    else if( json_value.is_array() )
    {
        const auto& json_array = json_value.as_array();
        if( json_array.size() > 0 )
        {
            json::value_type val_type = json_array[0].type();
            for( unsigned i = 0; i < json_array.size(); ++i )
            {
                if( json_array[i].type() == json::value_type::integer && val_type == json::value_type::real )
                {
                }
                else if( json_array[i].type() == json::value_type::real && val_type == json::value_type::integer )
                {
                    val_type = json::value_type::real;
                }
            }

            if( json_array[0].is_object() )
            {
                serializer_write_object_array( writer, member_name, json_array.size(), [&json_array]( ISerializer& s, unsigned idx )
                {
                    const json::object& object = json_array[idx].as_object();
                    for( auto it = object.begin(), end = object.end(); it != end; ++it )
                    {
                        json_to_other( it->second, it->first.c_str(), s );
                    }
                } );
            }
            else if( val_type == json::value_type::integer )
            {
                writer.serialize( member_name, JsonValueArray< int, &json::value::as_int >( json_array ) );
            }
            else if( val_type == json::value_type::string )
            {
                writer.serialize(member_name, JsonStringArray(json_array));
            }
            else if( val_type == json::value_type::boolean )
            {
                writer.serialize( member_name, JsonValueArray< bool, &json::value::as_bool >( json_array ) );
            }
            else if( val_type == json::value_type::real )
            {
                writer.serialize( member_name, JsonValueArray< float, &json::value::as_float >( json_array ) );
            }
        }
    }
    else if( json_value.is_int() )
    {
        serialize( writer, member_name, json_value.as_int() );
    }
    else if( json_value.is_string() )
    {
        const std::string str = json_value.as_string();
        serialize( writer, member_name, str.c_str(), str.length() );
    }
    else if( json_value.is_bool() )
    {
        serialize( writer, member_name, json_value.as_bool() );
    }
    else if( json_value.is_real() )
    {
        serialize( writer, member_name, json_value.as_float() );
    }
    else if ( json_value.is_null() )
    {
        // do nothing
    }
    else
    {
        SERIALIZER_ASSERT( false, "Unsupported type!" );
    }
}

void json_to_other( const json::value& json_value, ISerializer& writer )
{
    SERIALIZER_ASSERT( json_value.is_object(), "The serializer interface has an object at its root. If this is an array, please call `json_to_other( json_value, \"array name\", writer );`" );

    for( auto it = json_value.as_object().begin(), end = json_value.as_object().end(); it != end; ++it )
    {
        json_to_other( it->second, to_ss( it->first ), writer );
    }
}

// Compile json in this compilation unit to avoid adding dependencies to the project
#include "asielorz-json/parser.cc"
#include "asielorz-json/writer.cc"
#include "asielorz-json/value.cc"
