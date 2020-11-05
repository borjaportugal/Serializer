// Logic to read and write data in a custom binary format.
//
// Types that can be stored:
//      - basic types: int, unsigned int, float, bool, strings (as const char* and length pair)
//      - array: collection of elements of the same type.
//      - object: key/value elements, where the value can be any of the above (internally the key is an index into an string table)
// 
// Data layout (EH = ElementHeader, AH = ArrayHeader)
//      - For simple elements: EH|data
//      - For arrays: EH|AH|data
//      

#include "binary_serializer.h"

#include <string_view>
#include <istream>

#ifdef SERIALIZER_ENABLE_IO_FUNCTIONS
#include <fstream>
#endif

namespace
{
    constexpr unsigned c_writer_initial_size = 4098u;
    constexpr unsigned c_writer_growth_factor = 2;

    // #todo_borja Allow the user to be able to provide it's own allocation and deallocation methods for this memory
    std::unique_ptr< unsigned char[] > allocate_bytes( std::size_t size )
    {
        return std::make_unique< unsigned char[] >( size );
    }

    #pragma pack(push,1)
    struct ElementHeader
    {
        enum ElementType
        {
            Int,
            UInt,
            Float,
            Bool,
            String,
            Object,
            Array,
            Null,   // Used when a variable is overriden or to represent an array of binary data that the user understands.
                    // ElementHeader::m_type should never have this valye when writen to a file.

            Max
        };

        static_assert( ElementType::Max <= 8 );

        ElementHeader()
            : m_type{ Int }
            , m_size{ 0 }
            , m_name{ 0 }
        {}

        unsigned short m_type : 3;    // [0, 7] Type of this element
        unsigned short m_name : 13;   // [0, 8191] Name of the element (integer is converted to string usin the string table)
        unsigned m_size;   // Size in bytes of this object, including all sub-objects. Adding this size to the end of this element gives us the pointer where the next element starts.

        static constexpr unsigned max_name_idx = 8191;
    };

    constexpr inline bool is_numeric( ElementHeader::ElementType type )
    {
        static_assert( ElementHeader::ElementType::Int < ElementHeader::ElementType::Bool, "Order of the enum changed, update this function." );
        static_assert( ElementHeader::ElementType::UInt < ElementHeader::ElementType::Bool, "Order of the enum changed, update this function." );
        static_assert( ElementHeader::ElementType::Float < ElementHeader::ElementType::Bool, "Order of the enum changed, update this function." );
        return type <= ElementHeader::ElementType::Bool;
    }

    struct ArrayHeader
    {
        ArrayHeader()
            : ArrayHeader( ElementHeader::ElementType::Int, 0 )
        {}
        
        ArrayHeader( ElementHeader::ElementType inner_type, unsigned element_num )
            : m_inner_type{ static_cast< unsigned >( inner_type ) }
            , m_element_num{ element_num }
        {
            SERIALIZER_ASSERT( element_num < ArrayHeader::max_element_num, "" );
        }

        const unsigned m_inner_type : 3;
        const unsigned m_element_num : 29;

        static constexpr unsigned max_element_num = 536870911;  // #study_borja Can we avoid hardcoding this value?
    };
    #pragma pack(pop)

    // #todo_borja Make sure it has the same size in 32 and 64 bit architecture.
    static_assert( sizeof( ElementHeader ) == 6, "ElementHeader cannot change! All existing files will be invalidated." );
    static_assert( sizeof( ArrayHeader ) == 4, "ArrayHeader cannot change! All existing files will be invalidated." );

    // Helper class to read and handle simple value conversions.
    struct Numeric
    {
        union Value
        {
            int i;
            unsigned u;
            float f;
            bool b;
        };
        
        enum class Type
        {
            None,
            Int,
            UInt,
            Float,
            Bool,
        };
        
        Numeric() { m_value.u = 0; }
        explicit Numeric( int i ) : m_type{ Type::Int } { m_value.i = i; }
        explicit Numeric( unsigned u ) : m_type{ Type::UInt } { m_value.u = u; }
        explicit Numeric( float f ) : m_type{ Type::Float } { m_value.f = f; }
        explicit Numeric( bool b ) : m_type{ Type::Bool } { m_value.b = b; }

        void operator >>( int& var ) const;
        void operator >>( unsigned& var ) const;
        void operator >>( float& var ) const;
        void operator >>( bool& var ) const;

        Type m_type{ Type::None };
        Value m_value;
    };

    void Numeric::operator >>( int& var ) const
    {
        switch( m_type )
        {
            case Type::Int: var = m_value.i; break;
            case Type::UInt: var = static_cast< int >( m_value.u ); break;
            case Type::Float: var = static_cast< int >( m_value.f ); break;
            case Type::Bool: var = m_value.b ? 1 : 0; break;
        }
    }
    void Numeric::operator >>( unsigned& var ) const
    {
        switch( m_type )
        {
            case Type::Int: var = static_cast< unsigned >( m_value.i ); break;
            case Type::UInt: var = m_value.u; break;
            case Type::Float: var = static_cast< unsigned >( m_value.f ); break;
            case Type::Bool: var = m_value.b ? 1 : 0; break;
        }
    }
    void Numeric::operator >>( float& var ) const
    {
        switch( m_type )
        {
            case Type::Int: var = static_cast< float >( m_value.i ); break;
            case Type::UInt: var = static_cast< float >( m_value.u ); break;
            case Type::Float: var = m_value.f; break;
            case Type::Bool: var = m_value.b ? 1.f : 0.f; break;
        }
    }
    void Numeric::operator >>( bool& var ) const
    {
        switch( m_type )
        {
            case Type::Int: var = ( m_value.i != 0 ); break;
            case Type::UInt: var = ( m_value.u != 0 ); break;
            case Type::Float: var = ( m_value.f != 0 ); break;
            case Type::Bool: var = m_value.b; break;
        }
    }

    unsigned map_string_to_integer( std::vector< std::string >& strings,  const SerializerString str )
    {
        std::string_view strview{ str.m_string, str.m_length };
        const auto it = std::find( strings.begin(), strings.end(), strview );
        if( it != strings.end() )
        {
            const unsigned idx = std::distance( strings.begin(), it );
            return idx;
        }

        const unsigned idx = static_cast< unsigned >( strings.size() );
        SERIALIZER_ASSERT( idx <= ElementHeader::max_name_idx, "Too many names used for BinarySerializer. Max name: %d. Current name: %d", ElementHeader::max_name_idx, idx );
        strings.push_back( std::string{ str.m_string, str.m_length } );
        return idx;
    }
    
    void grow(std::unique_ptr< unsigned char[] >& data, std::size_t& used_size, std::size_t& data_size, std::size_t needed_size )
    {
        std::size_t new_data_size = data_size;
        do
        {
            new_data_size = new_data_size > 0 ? new_data_size * c_writer_growth_factor : c_writer_initial_size;
        } while ( new_data_size < used_size + needed_size );
        
        // #todo_borja Allow the user to be able to provide it's own allocation and deallocation methods for this memory
        std::unique_ptr< unsigned char[] > new_data = allocate_bytes(new_data_size);

        if (data)
            std::memcpy(new_data.get(), data.get(), used_size);

        data = std::move(new_data);
        data_size = new_data_size;
    }

    std::size_t reserve_bytes(std::unique_ptr< unsigned char[] >& data, std::size_t& used_size, std::size_t& data_size, std::size_t to_reserve)
    {
        if (used_size + to_reserve > data_size)
            grow(data, used_size, data_size, used_size + to_reserve);

        const std::size_t reserved_memory_start = used_size;
        used_size += to_reserve;
        return reserved_memory_start;
    }

    void write( std::unique_ptr< unsigned char[] >& data, std::size_t& used_size, std::size_t& data_size, const void* const to_write, const std::size_t to_write_size )
    {
        if( used_size + to_write_size > data_size )
        {
            grow(data, used_size, data_size, used_size + to_write_size);
        }
        
        std::memcpy( data.get() + used_size, to_write, to_write_size );
        used_size += to_write_size;
    }
    
    void nullify_elements_with_name( const unsigned short name_idx, unsigned char* const data, const std::size_t data_size )
    {
        unsigned char* data_it = data;
        const unsigned char* const data_end = data + data_size;
        while( data_it < data_end )
        {
            ElementHeader* const element = reinterpret_cast< ElementHeader* >( data_it );
            if( element->m_name == name_idx )
                element->m_type = ElementHeader::ElementType::Null;

            data_it += sizeof( ElementHeader ) + element->m_size;
        }        
    }

    unsigned remove_null_elements( unsigned char* const data, const unsigned data_size )
    {
        unsigned final_data_size = data_size;
        unsigned char* sub_element_it = data;
        unsigned char* proper_data = sub_element_it;
        const unsigned char* const sub_element_end = sub_element_it + data_size;
        while( sub_element_it < sub_element_end )
        {
            const ElementHeader* const sub_element_header = reinterpret_cast< const ElementHeader* >( sub_element_it );
            unsigned char* const next_element_it = sub_element_it + sizeof( ElementHeader ) + sub_element_header->m_size;
            if( sub_element_header->m_type == ElementHeader::ElementType::Null )
            {
                sub_element_it = next_element_it;
                final_data_size -= ( sizeof( ElementHeader ) + sub_element_header->m_size );
            }
            else
            {
                if( proper_data == sub_element_it )
                {
                    sub_element_it = next_element_it; 
                    proper_data = next_element_it;
                }
                else
                {
                    const std::size_t size_to_copy = sizeof( ElementHeader ) + sub_element_header->m_size;
                    
                    if( proper_data + size_to_copy <= sub_element_it )
                        std::memcpy( proper_data, sub_element_it, size_to_copy );
                    else // Memory is overlapping, behavior of std::memcpy is undefined
                    {
                        // #optimize_borja Copy as much data as we can using memory aligned to the type of vectorized_t
                        using vectorized_t = unsigned long long;
                        vectorized_t* dest = reinterpret_cast< vectorized_t* >( proper_data );
                        vectorized_t* src = reinterpret_cast< vectorized_t* >( sub_element_it );
                        const unsigned vector_size = size_to_copy / sizeof( vectorized_t );
                        for( unsigned i = 0; i < vector_size; ++i )
                        {
                            dest[i] = src[i];
                        }

                        // Copy the possible last few bytes
                        const unsigned last_written_byte = vector_size * sizeof( vectorized_t );
                        for( unsigned i = size_to_copy - last_written_byte; i < size_to_copy; ++i )
                        {
                            proper_data[i] = sub_element_it[i];
                        }
                    }
                    
                    proper_data += size_to_copy;
                    sub_element_it += size_to_copy;
                }
            }
        }

        SERIALIZER_ASSERT( final_data_size <= data_size, "" );
        return final_data_size;
    }

    template < typename T >
    void write_element( const unsigned short name_idx, std::unique_ptr< unsigned char[] >& data, std::size_t& used_size, std::size_t& data_size, const ElementHeader::ElementType type, const T var )
    {
        ElementHeader header;
        header.m_type = type;
        header.m_size = sizeof( T );
        header.m_name = name_idx;
        write( data, used_size, data_size, &header, sizeof( header ) );
        write( data, used_size, data_size, &var, header.m_size );
    }

    template < typename T >
    void write_array_element( const SerializerArray< T >& array, const ElementHeader::ElementType inner_type, const unsigned short name_idx, std::unique_ptr< unsigned char[] >& data, std::size_t& used_size, std::size_t& data_size )
    {
        const unsigned array_size = array.get_size();

        {
            ElementHeader header;
            header.m_type = ElementHeader::Array;
            header.m_size = sizeof( ArrayHeader ) + array_size * sizeof( T );
            header.m_name = name_idx;
            write( data, used_size, data_size, &header, sizeof( header ) );
        }

        {
            const ArrayHeader array_header{ inner_type, array_size };
            write( data, used_size, data_size, &array_header, sizeof( array_header ) );
        }

        if( array.supports_get_set_all() )
        {
            const T* const array_data = array.get_all();
            write( data, used_size, data_size, array_data, sizeof( T ) * array_size );
        }
        else
        {
            for( unsigned i = 0; i < array_size; ++i )
            {
                T t;
                array.get_element( i, t );
                write( data, used_size, data_size, &t, sizeof( T ) );
            }
        }
    }

    template < typename T >
    inline T read( const unsigned char*& src )
    {
        T t;
        std::memcpy( &t, src, sizeof( T ) );
        src += sizeof( T );
        return t;
    }

    Numeric read_numeric( const ElementHeader::ElementType& type, const unsigned char* data )
    {
        switch( type )
        {
            case ElementHeader::ElementType::Int: return Numeric{ read< int >( data ) };
            case ElementHeader::ElementType::UInt: return Numeric{ read< unsigned >( data ) };
            case ElementHeader::ElementType::Float: return Numeric{ read< float >( data ) };
            case ElementHeader::ElementType::Bool: return Numeric{ read< unsigned char >( data ) > 0 };
        };

        return Numeric{};
    }
    
    inline Numeric read_numeric( const ElementHeader& element )
    {
        return read_numeric( static_cast< ElementHeader::ElementType >( element.m_type ), reinterpret_cast< const unsigned char* >( &element + 1 ) );
    }

    template < typename T >
    void read_numeric_element( const SerializerString name, T& var, const std::vector< std::string >& strings, const unsigned char* const data, const std::size_t data_size )
    {
        if( const ElementHeader* const element = find_element( name, strings, data, data_size ) )
        {
            const Numeric numeric = read_numeric( *element );
            numeric >> var;
        }
    }
    
    void read_array( const ElementHeader& element_header, const SerializerArray< bool >& array )
    {
        const ArrayHeader& array_header = *reinterpret_cast< const ArrayHeader* >( &element_header + 1 );
        const char* const array_data = reinterpret_cast< const char* >( &array_header + 1 );

        array.set_size( array_header.m_element_num );
        for( unsigned i = 0; i < array_header.m_element_num; ++i )
        {
            array.set_element( i, array_data[i] > 0 );
        }
    }

    template < typename T >
    void read_array( const ElementHeader& element_header, const SerializerArray< T >& array )
    {
        SERIALIZER_ASSERT( element_header.m_type == ElementHeader::ElementType::Array, "" );

        const ArrayHeader& array_header = *reinterpret_cast< const ArrayHeader* >( &element_header + 1 );
        const T* const array_data = reinterpret_cast< const T* >( &array_header + 1 );
        
        const bool correct_type = (std::is_same_v< T, int > && array_header.m_inner_type == ElementHeader::ElementType::Int)
            || (std::is_same_v< T, unsigned > && array_header.m_inner_type == ElementHeader::ElementType::UInt)
            || (std::is_same_v< T, float > && array_header.m_inner_type == ElementHeader::ElementType::Float);

        if (correct_type)
        {
            if( array.supports_get_set_all() )
            {
                array.set_all( array_header.m_element_num, array_data );
            }
            else
            {
                array.set_size( array_header.m_element_num );
                for( unsigned i = 0; i < array_header.m_element_num; ++i )
                {
                    array.set_element( i, array_data[i] );
                }
            }
        }
        else
        {
            array.set_size(array_header.m_element_num);

            const unsigned char* array_data = reinterpret_cast< const unsigned char* >( &array_header + 1 );
            T t;
            for (unsigned i = 0; i < array_header.m_element_num; ++i)
            {
                const Numeric numeric = read_numeric(static_cast<ElementHeader::ElementType>(array_header.m_inner_type ), array_data + i * 4);
                numeric >> t;
                array.set_element(i, t);
            }
        }
        
    }

    template < typename T >
    void read_array_element( const SerializerString& name, const SerializerArray< T >& array, const std::vector< std::string >& strings, const unsigned char* const data, const std::size_t data_size )
    {
        const ElementHeader* const element_header = find_element( name, strings, data, data_size );
        if( !element_header )
            return;
        
        if( element_header->m_type == ElementHeader::ElementType::Array )
        {
            read_array( *element_header, array );
        }
        else
        {
            // #study_borja The logic of loading an element into an array should be done at a higher level. But for that we need an interface in ISerializer to know the type of the object we are going to load.
            SERIALIZER_ASSERT( element_header->m_type != ElementHeader::ElementType::Object, "Cannot load an object into an array of a built in type." );

            T var;
            const Numeric numeric = read_numeric( *element_header );
            numeric >> var;
            array.set_size( 1 );
            array.set_element( 0, var );
        }
    }
    
    const ElementHeader* find_element( const SerializerString name, const std::vector< std::string >& strings, const unsigned char* const data, const std::size_t data_size )
    {
        const unsigned char* data_it = data;
        const unsigned char* const data_end = data + data_size;
        while( data_it < data_end )
        {
            const ElementHeader* const element = reinterpret_cast< const ElementHeader* >( data_it );

            // #study_borja Consider checking the integer value instead of the strings. We would search for the integer value outside of the loop.
            //              Problem: right now the complexity to map the name to index is O(N) in the number of strings in the string table. If we do the change with current implementation it might end up in worse performance.
            //              Solution 1: Create a map from string to index (but that would mean allocating more memory during load and we want it to be as fast as possible)
            //              Solution 2: We could create a hash from the string and compare that in all places.
            const std::string& curr_element_name = strings[element->m_name];
            if( name == SerializerString{ curr_element_name.c_str(), curr_element_name.length() } )
                return element;

            data_it += sizeof( ElementHeader ) + element->m_size;
        }

        return nullptr;
    }

    template < typename T >
    T read( unsigned char*& data )
    {
        T t;
        std::memcpy( &t, data, sizeof( t ) );
        data += sizeof( t );
        return t;
    }

    template < typename T >
    void read( T* dest, unsigned char*& src, std::size_t size )
    {
        std::memcpy( dest, src, size * sizeof( T ) );
    }
}

void load_from_memory( const unsigned char* data, const unsigned data_size, BinaryData& out_data )
{
    const unsigned char* const data_end = data + data_size;

    const std::size_t string_num = read< std::size_t >( data );
    out_data.m_strings.reserve( string_num );
    
    std::string str;
    for( std::size_t i = 0; i < string_num; ++i )
    {
        const std::size_t string_size = read< std::size_t >( data );
        SERIALIZER_ASSERT( data < data_end, "" );

        str.resize( string_size, '\0' );
        std::memcpy( &str[0], data, string_size );
        data += string_size;
        SERIALIZER_ASSERT( data < data_end, "" );

        out_data.m_strings.emplace_back( std::move( str ) );
    }

    const auto out_data_size = read< std::size_t >( data );
    SERIALIZER_ASSERT( data + out_data_size <= data_end, "Invalid data size, this can cause a potential memory race." );

    out_data.m_data = data;
    out_data.m_data_size = out_data_size;
}

void save_to_memory( const BinaryDataHolder& data, std::unique_ptr< unsigned char[] >& out_data, std::size_t& out_used_size, std::size_t& out_data_size )
{
    const std::size_t string_num = data.m_strings.size();
    write( out_data, out_used_size, out_data_size, &string_num, sizeof( string_num ) );
    
    std::string str;
    for( std::size_t i = 0; i < string_num; ++i )
    {
        const std::string& str = data.m_strings[i];
        const std::size_t string_size = str.length();
        write( out_data, out_used_size, out_data_size, &string_size, sizeof( string_size ) );
        write( out_data, out_used_size, out_data_size, &str[0], string_size );
    }

    write( out_data, out_used_size, out_data_size, &data.m_used_size, sizeof( data.m_used_size ) );
    write( out_data, out_used_size, out_data_size, data.m_data.get(), data.m_used_size );
}

#ifdef SERIALIZER_ENABLE_IO_FUNCTIONS
namespace
{
    template < typename T >
    inline void write( std::ostream& os, const T t )
    {
        os.write( reinterpret_cast< const char* >( &t ), sizeof( t ) );
    }

    template < typename T >
    T read( std::istream& is, const T default_value )
    {
        T t;
        if( !is.read( reinterpret_cast< char* >( &t ), sizeof( t ) ) )
            return default_value;
        
        return t;
    }
}

void save( std::ostream& os, const BinaryDataHolder& holder )
{
    // Strings
    {
        write( os, holder.m_strings.size() );
        for( const std::string& str : holder.m_strings )
        {
            write( os, str.size() );
            os.write( str.c_str(), str.size() );
        }
    }

    write( os, holder.m_used_size );
    os.write( reinterpret_cast< char* >( holder.m_data.get() ), holder.m_used_size );
}

void load( std::istream& is, BinaryDataHolder& holder )
{
    // Strings
    {
        const std::size_t string_num = read< std::size_t >( is, 0 );
        holder.m_strings.reserve( string_num );
        
        std::string str;
        for( std::size_t i = 0; i < string_num; ++i )
        {
            const std::size_t string_size = read< std::size_t >( is, 0 );
            str.resize( string_size, '\n' );
            is.read( &str[0], string_size );
            holder.m_strings.emplace_back( std::move( str ) );
        }
    }

    const std::size_t data_size = read< std::size_t >( is, 0 );
    if( data_size > 0 )
    {
        // #todo_borja Allow the user to be able to provide it's own allocation and deallocation methods for this memory
        auto data = allocate_bytes( data_size );
        is.read( reinterpret_cast< char* >( data.get() ), data_size );

        holder.m_data = std::move( data );
        holder.m_data_size = data_size;
        holder.m_used_size = data_size;
    }
}

void save( const char* filename, const BinaryDataHolder& holder )
{
    std::ofstream fp{ filename, std::ofstream::binary };
    if( fp.is_open() )
        save( fp, holder );
}
void load( const char* filename, BinaryDataHolder& holder )
{
    std::ifstream fp{ filename, std::ofstream::binary };
    if( fp.is_open() )
        load( fp, holder );
}
#endif

BinaryWriter::~BinaryWriter()
{
    // #study_borja Consider moving this code to some other place. The user might not expect that cleanup is being done in the destruction of the writer.
    const unsigned prev_size = m_used_size - m_first_element_header_start;
    const unsigned new_size = remove_null_elements( reinterpret_cast< unsigned char* >( m_data.get() + m_first_element_header_start ), prev_size );
    m_used_size -= ( prev_size - new_size );
}

void BinaryWriter::write_memory_chunk( const SerializerString name, MemoryChunk chunk ) const
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );

    ElementHeader header;
    header.m_type = ElementHeader::ElementType::Array;
    header.m_size = sizeof( ArrayHeader ) + chunk.m_data_size;
    header.m_name = name_idx;

    // Null represents a chunk of binary data that the user understands
    const ArrayHeader array_header{ ElementHeader::ElementType::Null, static_cast< unsigned >( chunk.m_data_size ) };
    
    write( m_data, m_used_size, m_data_size, &header, sizeof( header ) );
    write( m_data, m_used_size, m_data_size, &array_header, sizeof( array_header ) );
    write( m_data, m_used_size, m_data_size, chunk.m_data, chunk.m_data_size );
}

bool BinaryWriter::is_reader() const
{
    return false;
}
bool BinaryWriter::has_member( const SerializerString name ) const
{
    const unsigned char* data_it = m_data.get() + m_first_element_header_start;
    const unsigned char* const data_end = m_data.get() + m_used_size;
    while (data_it < data_end)
    {
        const ElementHeader* const element = reinterpret_cast<const ElementHeader*>(data_it);
        if (element->m_type != ElementHeader::ElementType::Null)
        {
            const std::string& curr_element_name = m_strings[element->m_name];
            const SerializerString curr_serializer_name = { curr_element_name.c_str(), curr_element_name.length() };
            if (name == curr_serializer_name)
                return true;
        }

        data_it += sizeof(ElementHeader) + element->m_size;
    }

    return false;
}


void BinaryWriter::serialize( const SerializerString name, int& var )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    write_element( name_idx, m_data, m_used_size, m_data_size, ElementHeader::Int, var );
}

void BinaryWriter::serialize( const SerializerString name, unsigned int& var )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    write_element( name_idx, m_data, m_used_size, m_data_size, ElementHeader::UInt, var );
}

void BinaryWriter::serialize( const SerializerString name, float& var )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    write_element( name_idx, m_data, m_used_size, m_data_size, ElementHeader::Float, var );
}

void BinaryWriter::serialize( const SerializerString name, bool& var )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );

    const unsigned char c = var ? 1 : 0;
    ElementHeader header;
    header.m_type = ElementHeader::Bool;
    header.m_size = sizeof( c );
    header.m_name = name_idx;
    write( m_data, m_used_size, m_data_size, &header, sizeof( header ) );
    write( m_data, m_used_size, m_data_size, &c, header.m_size );
}

void BinaryWriter::serialize( const SerializerString name, const char*& str, unsigned& length )
{
    const unsigned idx = map_string_to_integer( m_strings, SerializerString{ str, length, false } );
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    write_element( name_idx, m_data, m_used_size, m_data_size, ElementHeader::String, idx );
}

void BinaryWriter::serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn )
{
    // Save some memory for us to write the size of the entire object
    const std::size_t element_header_start = reserve_bytes( m_data, m_used_size, m_data_size, sizeof( ElementHeader ) );
    
    // Save sub elements
    {
        BinaryWriter sub_object_writer{ m_strings, m_data, m_data_size, m_used_size };
        fn( sub_object_writer, user_data );
    }

    if( m_used_size == element_header_start + sizeof( ElementHeader ) )
    {
        // Nothing was written, no need to use any memory for the header of this object.
        m_used_size = element_header_start;
    }
    else
    {
        // We have sub elements, store the total size of the element (of all sub elements)
        ElementHeader* const header = reinterpret_cast< ElementHeader* >( m_data.get() + element_header_start );
        
        header->m_type = ElementHeader::Object;
        header->m_size = m_used_size - element_header_start - sizeof( ElementHeader );
        header->m_name = map_string_to_integer( m_strings, name );
    }
}

void BinaryWriter::iterate_elements( void* const user_data, const IterateElementsFn fn )
{
    const unsigned char* data_it = m_data.get() + m_first_element_header_start;
    const unsigned char* const data_end = m_data.get() + m_used_size;
    while( data_it < data_end )
    {
        const ElementHeader* const element = reinterpret_cast< const ElementHeader* >( data_it );
        if( element->m_type != ElementHeader::ElementType::Null )
        {
            const std::string& curr_element_name = m_strings[element->m_name];
            const SerializerString serializer_name = { curr_element_name.c_str(), curr_element_name.length() };
            if( !fn( *this, serializer_name, user_data ) )
                break;
        }

        data_it += sizeof( ElementHeader ) + element->m_size;
    }
}

void BinaryWriter::serialize( const SerializerString name, const SerializerArray< int >& array )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    write_array_element( array, ElementHeader::Int, name_idx, m_data, m_used_size, m_data_size );
}

void BinaryWriter::serialize( const SerializerString name, const SerializerArray< unsigned int >& array )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    write_array_element( array, ElementHeader::UInt, name_idx, m_data, m_used_size, m_data_size );
}

void BinaryWriter::serialize( const SerializerString name, const SerializerArray< float >& array )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    write_array_element( array, ElementHeader::Float, name_idx, m_data, m_used_size, m_data_size );
}

void BinaryWriter::serialize( const SerializerString name, const SerializerArray< bool >& array )
{
    // #optimize_borja (memory) Write as bits
    
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    const unsigned array_size = array.get_size();

    {
        ElementHeader header;
        header.m_type = ElementHeader::Array;
        header.m_size = sizeof( ArrayHeader ) + array_size * sizeof( char );
        header.m_name = name_idx;
        write( m_data, m_used_size, m_data_size, &header, sizeof( header ) );
    }

    {
        const ArrayHeader array_header{ ElementHeader::ElementType::Bool, array_size };
        write( m_data, m_used_size, m_data_size, &array_header, sizeof( array_header ) );
    }

    for( unsigned i = 0; i < array_size; ++i )
    {
        bool b = false;
        array.get_element( i, b );
        const char c = b ? 1 : 0;
        write( m_data, m_used_size, m_data_size, &c, sizeof( c ) );
    }
}

void BinaryWriter::serialize( const SerializerString name, const SerializerArray< SerializerString >& array )
{
    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, m_used_size - m_first_element_header_start );
    const unsigned array_size = array.get_size();

    {
        ElementHeader header;
        header.m_type = ElementHeader::Array;
        header.m_size = sizeof( ArrayHeader ) + array_size * sizeof( unsigned );
        header.m_name = name_idx;
        write( m_data, m_used_size, m_data_size, &header, sizeof( header ) );
    }

    {
        const ArrayHeader array_header{ ElementHeader::ElementType::String, array_size };
        write( m_data, m_used_size, m_data_size, &array_header, sizeof( array_header ) );
    }

    for( unsigned i = 0; i < array_size; ++i )
    {
        SerializerString ss;
        array.get_element( i, ss );
        
        const unsigned str_idx = map_string_to_integer( m_strings, ss );
        write( m_data, m_used_size, m_data_size, &str_idx, sizeof( str_idx ) );
    }
}

void BinaryWriter::write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn )
{
    // Save memory for us to write the size of the entire object
    const std::size_t element_header_start = reserve_bytes( m_data, m_used_size, m_data_size, sizeof( ElementHeader ) + sizeof( ArrayHeader ) );
    
    // Save sub elements
    for( unsigned i = 0; i < element_num; ++i )
    {
        const std::size_t object_size_start = reserve_bytes( m_data, m_used_size, m_data_size, sizeof( unsigned ) );

        {
            BinaryWriter sub_object_writer{ m_strings, m_data, m_data_size, m_used_size };
            fn( sub_object_writer, i, user_data );
        }

        // Store the size of this object. Size of 0 is allowed, it means nothing was saved for that object (think as null object)
        unsigned* const object_size = reinterpret_cast< unsigned* >( m_data.get() + object_size_start );
        *object_size = static_cast< unsigned >( m_used_size - object_size_start - sizeof( unsigned ) );
    }

    const unsigned short name_idx = map_string_to_integer( m_strings, name );
    nullify_elements_with_name( name_idx, m_data.get() + m_first_element_header_start, element_header_start - m_first_element_header_start );

    if( m_used_size == element_header_start + sizeof( ElementHeader ) + sizeof( ArrayHeader ) + sizeof( unsigned ) * element_num )
    {
        // Nothing was written, no need to use any memory for the header of this object.
        m_used_size = element_header_start;
    }
    else
    {
        // We have sub elements, store the total size of the element (of all sub elements)
        ElementHeader* const element_header = reinterpret_cast< ElementHeader* >( m_data.get() + element_header_start );
        element_header->m_type = ElementHeader::Array;
        element_header->m_size = m_used_size - element_header_start - sizeof( ElementHeader );
        element_header->m_name = name_idx;

        ArrayHeader* const array_header = reinterpret_cast< ArrayHeader* >( element_header + 1 );
        new(array_header) ArrayHeader{ ElementHeader::ElementType::Object, element_num };
    }
}
unsigned BinaryWriter::read_object_array_size( const SerializerString name )
{
    SERIALIZER_ASSERT( false, "Not supported!" );
    return 0;
}
void BinaryWriter::read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn )
{
    SERIALIZER_ASSERT( false, "Not supported!" );
}



MemoryChunk BinaryReader::read_memory_chunk( const SerializerString name ) const
{
    const ElementHeader* const element = find_element( name, m_strings, m_data, m_data_size );
    if( !element || element->m_type != ElementHeader::ElementType::Array )
        return MemoryChunk{};
    
    const ArrayHeader* const array_header = reinterpret_cast< const ArrayHeader* >( element + 1 );
    if( array_header->m_inner_type != ElementHeader::ElementType::Null )
        return MemoryChunk{};
    
    MemoryChunk memory_chunk;
    memory_chunk.m_data = reinterpret_cast< const unsigned char* >( array_header + 1 );
    memory_chunk.m_data_size = array_header->m_element_num;
    return memory_chunk;
}

bool BinaryReader::is_reader() const
{
    return true;
}
bool BinaryReader::has_member(const SerializerString name) const
{
    return find_element(name, m_strings, m_data, m_data_size) != nullptr;
}

void BinaryReader::serialize( const SerializerString name, int& var )
{
    read_numeric_element(  name, var, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, unsigned int& var )
{
    read_numeric_element(  name, var, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, float& var )
{
    read_numeric_element(  name, var, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, bool& var )
{
    read_numeric_element(  name, var, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, const char*& out_str, unsigned& out_length )
{
    const ElementHeader* const element = find_element( name, m_strings, m_data, m_data_size );
    if( !element || element->m_type != ElementHeader::ElementType::String )
        return;
    
    const unsigned string_idx = *reinterpret_cast< const unsigned* >( element + 1 );
    const std::string& str = m_strings[string_idx];
    out_str = str.c_str();
    out_length = str.length();
}

void BinaryReader::serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn )
{
    if( const ElementHeader* const element = find_element( name, m_strings, m_data, m_data_size ) )
    {
        if( element->m_type != ElementHeader::ElementType::Object )
            return;
        
        BinaryReader sub_object_reader{ m_strings, reinterpret_cast< const unsigned char* >( element + 1 ), element->m_size };
        fn( sub_object_reader, user_data );
    }
}

void BinaryReader::iterate_elements( void* const user_data, const IterateElementsFn fn )
{
    const unsigned char* data_it = m_data;
    const unsigned char* const data_end = m_data + m_data_size;
    while( data_it < data_end )
    {
        const ElementHeader* const element = reinterpret_cast< const ElementHeader* >( data_it );

        const std::string& curr_element_name = m_strings[element->m_name];
        if( !fn( *this, SerializerString{ curr_element_name.c_str(), curr_element_name.length(), true }, user_data ) )
        {
            break;
        }

        data_it += sizeof( ElementHeader ) + element->m_size;
    }
}

void BinaryReader::serialize( const SerializerString name, const SerializerArray< int >& array )
{
    read_array_element( name, array, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, const SerializerArray< unsigned int >& array )
{
    read_array_element( name, array, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, const SerializerArray< float >& array )
{
    read_array_element( name, array, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, const SerializerArray< bool >& array )
{
    read_array_element( name, array, m_strings, m_data, m_data_size );
}

void BinaryReader::serialize( const SerializerString name, const SerializerArray< SerializerString >& array )
{
    const ElementHeader* const element_header = find_element( name, m_strings, m_data, m_data_size );
    if( !element_header )
        return;
    
    if( element_header->m_type == ElementHeader::ElementType::Array )
    {
        const ArrayHeader& array_header = *reinterpret_cast< const ArrayHeader* >( element_header + 1 );
        const unsigned* const array_data = reinterpret_cast< const unsigned* >( &array_header + 1 );

        array.set_size( array_header.m_element_num );
        for( unsigned i = 0; i < array_header.m_element_num; ++i )
        {
            const std::string& str = m_strings[array_data[i]];
            array.set_element( i, SerializerString{ str.c_str(), str.length() } );
        }
    }
    else if( element_header->m_type == ElementHeader::ElementType::String )
    {
        // #study_borja The logic of loading an element into an array should be done at a higher level. But for that we need an interface in ISerializer to know the type of the object we are going to load.
        const unsigned str_idx = *reinterpret_cast< const unsigned* >( element_header + 1 );
        const std::string& str = m_strings[str_idx];
        array.set_size( 1 );
        array.set_element( 0, SerializerString{ str.c_str(), str.length() } );
    }
}

void BinaryReader::write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn )
{
    SERIALIZER_ASSERT( false, "Not supported!" );
}
unsigned BinaryReader::read_object_array_size( const SerializerString name )
{
    const ElementHeader* const element_header = find_element( name, m_strings, m_data, m_data_size );
    if( !element_header || element_header->m_type != ElementHeader::ElementType::Array )
        return 0;

    const ArrayHeader& array_header = *reinterpret_cast< const ArrayHeader* >( element_header + 1 );
    SERIALIZER_ASSERT( array_header.m_inner_type == ElementHeader::ElementType::Object, "" );
    return array_header.m_element_num;
}
void BinaryReader::read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn )
{
    const ElementHeader* const element_header = find_element( name, m_strings, m_data, m_data_size );
    if( !element_header )
        return;

    if( element_header->m_type == ElementHeader::ElementType::Array )
    {
        const ArrayHeader& array_header = *reinterpret_cast< const ArrayHeader* >( element_header + 1 );
        SERIALIZER_ASSERT( array_header.m_inner_type == ElementHeader::ElementType::Object, "" );

        const unsigned char* const array_data_start = reinterpret_cast< const unsigned char* >( &array_header + 1 );
        const unsigned char* ptr_it = array_data_start;
        for( unsigned i = 0; i < array_header.m_element_num; ++i )
        {
            SERIALIZER_ASSERT( ptr_it <= array_data_start + element_header->m_size - sizeof( ArrayHeader ), "Corrupted array data." );

            const unsigned object_size = *reinterpret_cast< const unsigned* >( ptr_it );
            ptr_it += sizeof( unsigned );

            if( object_size > 0 )
            {
                const unsigned char* const object_data = ptr_it;

                BinaryReader sub_object_reader{ m_strings, object_data, object_size };
                fn( sub_object_reader, i, user_data );

                ptr_it += object_size;
            }
            // else this element of the array is a null element
        }
    }
}

void write_sub_binary_holder( BinaryWriter& writer, const SerializerString name, const BinaryDataHolder& holder )
{
    std::unique_ptr< unsigned char[] > data;
    std::size_t used_size = 0;
    std::size_t data_size = 0;
    save_to_memory( holder, data, used_size, data_size );
    
    MemoryChunk chunk;
    chunk.m_data = data.get();
    chunk.m_data_size = used_size;
    writer.write_memory_chunk( name, chunk );
}

BinaryData read_sub_binary_holder( BinaryReader& reader, const SerializerString name )
{
    const MemoryChunk memory_chunk = reader.read_memory_chunk( name );
    BinaryData data;
    load_from_memory( memory_chunk.m_data, memory_chunk.m_data_size, data );
    return data;
}


namespace
{
    template < typename T >
    struct ArrayConverter : SerializerArray< T >
    {
        ArrayConverter( const T* const data, const unsigned size )
            : m_data{ data }
            , m_size{ size }
        {} 

        unsigned get_size() const override
        {
            return m_size;
        }
        void get_element( unsigned i, T& t ) const override
        {
            SERIALIZER_ASSERT( i < get_size(), "" );
            t = m_data[i];
        }
        
        void set_size( unsigned i ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        void set_element( unsigned i, T t ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        
        bool supports_get_set_all() const override
        {
            return true;
        }
        const T* get_all() const override
        {
            return m_data;
        }
        void set_all( unsigned size, const T* const data ) const override
        { 
            SERIALIZER_ASSERT( false, "Not supported!" );
        }

        const T* const m_data{ nullptr };
        const unsigned m_size{ 0 };
    };

    struct BoolArrayConverter : SerializerArray< bool >
    {
        BoolArrayConverter( const char* const data, const unsigned size )
            : m_data{ data }
            , m_size{ size }
        {} 

        unsigned get_size() const override
        {
            return m_size;
        }
        void get_element( unsigned i, bool& t ) const override
        {
            SERIALIZER_ASSERT( i < get_size(), "" );
            t = m_data[i] > 0;
        }
        
        void set_size( unsigned i ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        void set_element( unsigned i, bool t ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        
        const char* const m_data{ nullptr };
        const unsigned m_size{ 0 };
    };
    
    struct StringArrayConverter : SerializerArray< SerializerString >
    {
        StringArrayConverter( const std::vector< std::string >& strings, const unsigned* const data, const unsigned size )
            : m_strings{ strings }
            , m_data{ data }
            , m_size{ size }
        {} 

        unsigned get_size() const override
        {
            return m_size;
        }
        void get_element( unsigned i, SerializerString& t ) const override
        {
            SERIALIZER_ASSERT( i < get_size(), "" );
            const unsigned string_idx = m_data[i];
            const std::string& str = m_strings[string_idx];
            t = SerializerString{ str.c_str(), str.length() };
        }
        
        void set_size( unsigned i ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        void set_element( unsigned i, SerializerString t ) const override
        {
            SERIALIZER_ASSERT( false, "Shouldn't be called!" );
        }
        
        const std::vector< std::string >& m_strings;
        const unsigned* const m_data{ nullptr };
        const unsigned m_size{ 0 };
    };

    void binary_to_other( const std::vector< std::string >& strings, const ElementHeader& element_header, ISerializer& writer );

    void binary_to_other( const std::vector< std::string >& strings, const unsigned char* const obj_data, const unsigned obj_size, ISerializer& writer )
    {
        const unsigned char* data_it = obj_data;
        while( data_it < obj_data + obj_size )
        {
            const ElementHeader& sub_element_header = *reinterpret_cast< const ElementHeader* >( data_it );
            binary_to_other( strings, sub_element_header, writer );
            data_it += sizeof( ElementHeader ) + sub_element_header.m_size;
        }
    }

    void binary_to_other( const std::vector< std::string >& strings, const ElementHeader& element_header, ISerializer& writer )
    {
        SERIALIZER_ASSERT( !writer.is_reader(), "Expecting a writer." );

        const std::string& string_from_string_table = strings[element_header.m_name];
        const SerializerString element_name{ string_from_string_table.c_str(), string_from_string_table.length() };

        if( is_numeric( static_cast< ElementHeader::ElementType >( element_header.m_type ) ) )
        {
            const Numeric numeric = read_numeric( element_header );
            if( numeric.m_type == Numeric::Type::Int )
                serialize( writer, element_name, numeric.m_value.i );
            else if( numeric.m_type == Numeric::Type::UInt )
                serialize( writer, element_name, numeric.m_value.u );
            else if( numeric.m_type == Numeric::Type::Float )
                serialize( writer, element_name, numeric.m_value.f );
            else if( numeric.m_type == Numeric::Type::Bool )
                serialize( writer, element_name, numeric.m_value.b );
        }
        else if( element_header.m_type == ElementHeader::ElementType::Object )
        {
            serialize_object( writer, element_name, [&strings, &element_header]( ISerializer& writer )
            {
                const unsigned char* const sub_element_data = reinterpret_cast< const unsigned char* >( &element_header + 1 );
                binary_to_other( strings, sub_element_data, element_header.m_size, writer );
            } );
        }
        else if( element_header.m_type == ElementHeader::ElementType::Array )
        {
            const ArrayHeader& array_header = *reinterpret_cast< const ArrayHeader* >( &element_header + 1 );
            const unsigned char* array_data_begin_it = reinterpret_cast< const unsigned char* >( &array_header + 1 );
            if( array_header.m_inner_type == ElementHeader::ElementType::Int )
            {
                writer.serialize( element_name, ArrayConverter< int >( reinterpret_cast< const int* >( array_data_begin_it ), array_header.m_element_num ) );
            }
            else if( array_header.m_inner_type == ElementHeader::ElementType::UInt )
            {
                writer.serialize( element_name, ArrayConverter< unsigned >( reinterpret_cast< const unsigned* >( array_data_begin_it ), array_header.m_element_num ) );
            }
            else if( array_header.m_inner_type == ElementHeader::ElementType::Float )
            {
                writer.serialize( element_name, ArrayConverter< float >( reinterpret_cast< const float* >( array_data_begin_it ), array_header.m_element_num ) );
            }
            else if( array_header.m_inner_type == ElementHeader::ElementType::Bool )
            {
                writer.serialize( element_name, BoolArrayConverter( reinterpret_cast< const char* >( array_data_begin_it ), array_header.m_element_num ) );
            }
            else if( array_header.m_inner_type == ElementHeader::ElementType::String )
            {
                writer.serialize( element_name, StringArrayConverter( strings, reinterpret_cast< const unsigned* >( array_data_begin_it ), array_header.m_element_num ) );
            }
            else if( array_header.m_inner_type == ElementHeader::ElementType::Object )
            {
                serializer_write_object_array( writer, element_name, array_header.m_element_num, [&strings, array_data_begin_it]( ISerializer& s, unsigned idx )
                {
                    const unsigned char* data_it = array_data_begin_it;
                    while( idx > 0 )
                    {
                        const unsigned obj_size = *reinterpret_cast< const unsigned* >( data_it );
                        data_it += sizeof( unsigned ) + obj_size;
                        idx--;
                    }

                    const unsigned obj_size = *reinterpret_cast< const unsigned* >( data_it );
                    const unsigned char* const obj_data = data_it + sizeof( unsigned );
                    binary_to_other( strings, obj_data, obj_size, s );
                } );
            }
        }
        else if( element_header.m_type == ElementHeader::ElementType::String )
        {
            const unsigned string_idx = *reinterpret_cast< const unsigned* >( &element_header + 1 );
            const std::string& str = strings[string_idx];
            serialize( writer, element_name, str.c_str(), str.length() );
        }
        else if( element_header.m_type == ElementHeader::ElementType::Null )
        {
            SERIALIZER_ASSERT( false, "Not expecting null elements!" );
        }
        else
        {
            SERIALIZER_ASSERT( false, "Unexpected element type!" );
        }
    }
}

void binary_to_other( const BinaryDataHolder& data_holder, ISerializer& writer )
{
    binary_to_other( data_holder.m_strings, data_holder.m_data.get(), data_holder.m_data_size, writer );
}
