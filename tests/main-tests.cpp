// This file contains tests to ensure that all serializer implementations behave in the same way.
// The tests are implemented in a template class so that they can be used by any serializer implementation.
// 
// Contents of the file:
//      - Structures and functions used by the tests.
//      - SerializerTests
//      - ConversionTest
//      - PerformanceTest
//      - Main test functions
//      - main

#include "json_serializer.h"
#include "asielorz-json/value.hh"
#include "asielorz-json/parser.hh"
#include "asielorz-json/writer.hh"

#include "binary_serializer.h"
#include "serializer_std.h"

#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <assert.h>

// 
// Hook up of load/save functions used in some tests.
// The compiler will select the correct one with overload resolution, see `SerializerTests` for more info.
// 

// json
void load( const char* filename, json::value& value )
{
    std::ifstream f{ filename };
    if( f.is_open() )
        value = json::parser::parse( f );
}
void save( const char* filename, const json::value& value )
{
    std::ofstream f{ filename };
    if( f.is_open() )
        f << json::writer::write( value );
}

// Binary
void load( const char* filename, BinaryDataHolder& value )
{
    std::ifstream f{ filename, std::ifstream::binary };
    if( f.is_open() )
        load( f, value );
}
void save( const char* filename, const BinaryDataHolder& value )
{
    std::ofstream f{ filename, std::ifstream::binary };
    if( f.is_open() )
        save( f, value );
}

// 
// Structures and functions used by the tests.
// 

struct Variables
{
    int a{ 0 };
    unsigned int b{ 0u };
    float c{ 0.f };
    bool d = false;
};
bool operator==( const Variables& lhs, const Variables& rhs )
{
    return lhs.a == rhs.a
    && lhs.b == rhs.b
    && lhs.c == rhs.c
    && lhs.d == rhs.d;
}
void serialize( ISerializer& serializer, Variables& variables )
{
    serializer.serialize( "B"_ss, variables.b );
    serializer.serialize( "A"_ss, variables.a );
    serializer.serialize( "C"_ss, variables.c );
    serializer.serialize( "D"_ss, variables.d );
}

struct Hierarchy
{
    int a{ 0 };
    std::unique_ptr< Hierarchy > child;
};
std::unique_ptr< Hierarchy > build_hierarchy( int num )
{
    auto h = std::make_unique< Hierarchy >();
    h->a = num;

    if( num == 0 )
    {
        return h;
    }

    h->child = build_hierarchy( num - 1 );
    return h;
}
void serialize( ISerializer& serializer, Hierarchy& h )
{
    serializer.serialize( "aaa"_ss, h.a );

    serialize_object( serializer, "child"_ss, [&h]( ISerializer& serializer )
    {
        if( serializer.is_reader() )
        {
            h.child = std::make_unique< Hierarchy >();
            serialize( serializer, *h.child );
        }
        else
        {
            if( h.child ) 
                serialize( serializer, *h.child );
        }
    } );
}
void serialize( ISerializer& s, const SerializerString name, std::vector< std::shared_ptr< Hierarchy > >& hierarchies )
{
    if( s.is_reader() )
    {
        const unsigned size = s.read_object_array_size( "hierarchies" );
        hierarchies.resize( size );
        serializer_read_object_array( s, "hierarchies", [&hierarchies]( ISerializer& s, unsigned idx )
        {
            hierarchies[idx] = std::make_unique< Hierarchy >();
            serialize( s, *hierarchies[idx] );
        } );
    }
    else
    {
        serializer_write_object_array( s, "hierarchies", hierarchies.size(), [&hierarchies]( ISerializer& s, unsigned idx )
        {
            auto& h = hierarchies[idx];
            if( h )
                serialize( s, *h );
        } );
    }
}
void serialize( ISerializer& s, const SerializerString name, const std::vector< std::shared_ptr< Hierarchy > >& v )
{
    auto copy = v;  // Lazy implementation, we don't care much about performance in these tests.
                    // Other option would be to do a `const_cast` to remove the constness. 
                    // In this case we want to make sure no changes are done to `v` due to bad Serializer implementation.
    serialize( s, name, copy );
}

bool operator==(const Hierarchy& lhs, const Hierarchy& rhs)
{
    if( lhs.a != rhs.a )
        return false;
    
    if( lhs.child && rhs.child )
        return *lhs.child == *rhs.child;

    return lhs.child == nullptr && rhs.child == nullptr;
}

#include "serializer_ext.h"
void test_equal_hierarchies(const std::vector< std::shared_ptr< Hierarchy > >& a, const std::vector< std::shared_ptr< Hierarchy > >& b )
{
    int aa[10] = {};
    unsigned sasda = 0;
    RawArraySerializer< int >( aa, aa, 10, &sasda );

    assert( a.size() == b.size() );
    for( std::size_t i = 0; i < a.size(); ++i )
    {
        if( a[i] && b[i] )
        {
            assert( *a[i] == *b[i] );
        }
        else
        {
            assert( a[i] == nullptr );
            assert( b[i] == nullptr );
        }
    }
}

// Helper function used by SerializerTests
void do_iterate_elements_test( ISerializer& serializer, std::vector< std::string > expected_element_names )
{
    std::vector< std::string > iterated_element_names;
    iterate_elements( serializer, [&iterated_element_names]( ISerializer& serializer, const SerializerString name )
    {
        iterated_element_names.push_back( to_std_string( name ) );
        return true;
    } );

    assert( expected_element_names.size() == iterated_element_names.size() );
    std::sort( expected_element_names.begin(), expected_element_names.end() );
    std::sort( iterated_element_names.begin(), iterated_element_names.end() );
    for( std::size_t i = 0; i < expected_element_names.size(); ++i )
    {
        assert( expected_element_names[i] == iterated_element_names[i] );
    }
}


// SerializerTests
// 
// Structure holding all the tests for specific serializers.
// This way we can verify that all specializations of serializers have the same behavior.
//
// In order for this functions to compile, three global functions need to be implemented:
//  - `void load( const char* filename, ValueHolder& out_value )`
//  - `void save( const char* filename, const ValueHolder& value )`
//  - 
template < class Reader, class Writer, class ValueHolder >
struct SerializerTests
{
    static void test_serialize_deserialize()
    {
        Variables original_variables;
        original_variables.a = 400;
        original_variables.b = 123456789;
        original_variables.c = 123.456789f;
        original_variables.d = true;

        ValueHolder value;
        {
            Writer writer{ value };
            serialize( writer, original_variables );
        }
        //print( value );

        Variables loaded_variables;
        {
            Reader reader{ value };
            serialize( reader, loaded_variables );
        }

        assert( original_variables.a == loaded_variables.a );
        assert( original_variables.b == loaded_variables.b );
        assert( original_variables.c == loaded_variables.c );
        assert( original_variables.d == loaded_variables.d );
    }

    static void test_value_conversion()
    {
        ValueHolder value;
        {
            int i = 21;
            unsigned u = 34;
            float f = 12.4f;
            bool b = true;

            Writer writer{ value };            
            writer.serialize( "i", i );
            writer.serialize( "u", u );
            writer.serialize( "f", f );
            writer.serialize( "b", b );
        }
        
        Reader reader{ value };

        // int conversion
        {
            unsigned u = 0;
            float f = 0;
            bool b = 0;
            
            reader.serialize( "i", u );
            reader.serialize( "i", f );
            reader.serialize( "i", b );
            
            assert( u == 21 );
            assert( f == 21 );
            assert( b == true );
        }
        
        // unsigned conversion
        {
            int i = 0;
            float f = 0;
            bool b = false;
            
            reader.serialize( "u", i );
            reader.serialize( "u", f );
            reader.serialize( "u", b );
            
            assert( i == 34 );
            assert( f == 34 );
            assert( b == true );
        }

        // float conversion
        {
            int i = 0;
            unsigned u = 0;
            bool b = false;

            reader.serialize( "f", i );
            reader.serialize( "f", u );
            reader.serialize( "f", b );
        
            assert( i == 12 );
            assert( u == 12 );
            assert( b == true );
        }

        // bool conversion
        {
            int i = 0;
            unsigned u = 0;
            float f = 0;

            reader.serialize( "b", i );
            reader.serialize( "b", u );
            reader.serialize( "b", f );
        
            assert( i == 1 );
            assert( u == 1 );
            assert( f == 1.f );
        }
    }
    
    static void test_value_conversion_global_functions()
    {
        ValueHolder value;
        {
            Writer writer{ value };            
            serialize( writer, "i", 21 );
            serialize( writer, "u", 34u );
            serialize( writer, "f", 12.4f );
            serialize( writer, "b", true );
        }
        
        Reader reader{ value };

        // int conversion
        {
            unsigned u = 0;
            float f = 0;
            bool b = 0;
            
            serialize( reader, "i", u );
            serialize( reader, "i", f );
            serialize( reader, "i", b );
            
            assert( u == 21 );
            assert( f == 21 );
            assert( b == true );
        }
        
        // unsigned conversion
        {
            int i = 0;
            float f = 0;
            bool b = false;
            
            serialize( reader, "u", i );
            serialize( reader, "u", f );
            serialize( reader, "u", b );
            
            assert( i == 34 );
            assert( f == 34 );
            assert( b == true );
        }

        // float conversion
        {
            int i = 0;
            unsigned u = 0;
            bool b = false;

            serialize( reader, "f", i );
            serialize( reader, "f", u );
            serialize( reader, "f", b );
        
            assert( i == 12 );
            assert( u == 12 );
            assert( b == true );
        }

        // bool conversion
        {
            int i = 0;
            unsigned u = 0;
            float f = 0;

            serialize( reader, "b", i );
            serialize( reader, "b", u );
            serialize( reader, "b", f );
        
            assert( i == 1 );
            assert( u == 1 );
            assert( f == 1.f );
        }
    }

    static void test_try_to_load_non_existent_variable()
    {
        int i = -12;
        unsigned u = 45;
        float f = 3.45f;
        bool b = true;

        ValueHolder empty;
        Reader reader{ empty };
        reader.serialize( "aaaaa", i );
        reader.serialize( "aaaaa", u );
        reader.serialize( "aaaaa", f );
        reader.serialize( "aaaaa", b );
        serialize_object( reader, "aaaaa", []( ISerializer& serializer )
        {
            assert( false );
        } );

        assert( i == -12 );
        assert( u == 45 );
        assert( f == 3.45f );
        assert( b == true );
    }

    static void test_empty_elements_are_not_saved()
    {
        // #study_borja We might want to store objects even if they didn't save any data. The user called to serialize with them, they should be shown.

        ValueHolder value;
        {
            Writer writer{ value };
            serialize_object( writer, "a", []( ISerializer& )
            {
                // do nothing, we just want to make sure that no element is created
            } );
        }

        {
            Reader reader{ value };
            iterate_elements( reader, []( ISerializer& serializer, const SerializerString name )
            {
                assert( false );
                return true;
            } );
        }
    }

    static void test_hiearchy()
    {
        std::unique_ptr< Hierarchy > root = build_hierarchy( 10 );

        ValueHolder value;
        {
            Writer writer{ value };
            serialize( writer, *root );
        }

        Hierarchy loaded_hierarchy;
        {
            Reader reader{ value };
            serialize( reader, loaded_hierarchy );
        }

        assert( *root == loaded_hierarchy );

    }

    static void test_iterate_elements()
    {
        Variables variables;
        variables.a = 1;
        variables.b = 3;
        variables.d = true;

        ValueHolder value;
        {
            Writer writer{ value };
            serialize( writer, variables );
        }

        std::map< std::string, int > loaded_vars;

        Reader reader{ value };
        iterate_elements( reader, [&loaded_vars]( ISerializer& serializer, const SerializerString name )
        {
            int value = 0;
            serializer.serialize( name, value );
            //std::cout << name.m_string << "=" << value << '\n';

            loaded_vars[ name.m_string ] = value;
            return true;
        } );

        assert( loaded_vars.size() == 4 );
        assert( loaded_vars["A"] == 1 );
        assert( loaded_vars["B"] == 3 );
        assert( loaded_vars["C"] == 0 );
        assert( loaded_vars["D"] == 1 );
    }
    
    static void test_iterate_elements_in_writer_and_with_objects()
    {
        ValueHolder value;
        {
            Writer writer{ value };
            serialize( writer, "i", -21 );
            do_iterate_elements_test( writer, { "i" } );
            serialize( writer, "u", 34u );
            do_iterate_elements_test( writer, { "i", "u" } );
            serialize( writer, "f", 12.4f );
            do_iterate_elements_test( writer, { "i", "u", "f" } );
            writer.serialize( "o", (void*)nullptr, []( ISerializer& serializer, void* user_data )
            {
                serialize( serializer, "i", -21 );
                do_iterate_elements_test( serializer, { "i" } );
                serialize( serializer, "u", 34u );
                do_iterate_elements_test( serializer, { "i", "u" } );
                serialize( serializer, "f", 12.4f );
                do_iterate_elements_test( serializer, { "i", "u", "f" } );
                serialize( serializer, "b", true );
                do_iterate_elements_test( serializer, { "i", "u", "f", "b" } );
            } );
            do_iterate_elements_test( writer, { "i", "u", "f", "o" } );
            serialize( writer, "b", true );
            do_iterate_elements_test( writer, { "i", "u", "f", "b", "o" } );
        }

        {
            Reader reader{ value };
            iterate_elements( reader, []( ISerializer& serializer, const SerializerString name )
            {
                do_iterate_elements_test( serializer, { "i", "u", "f", "b", "o" } );
                serializer.serialize( "o", (void*)nullptr, []( ISerializer& serializer, void* user_data )
                {
                    do_iterate_elements_test( serializer, { "i", "u", "f", "b" } );
                } );
                return true;
            } );
        }
    }

    static void test_strings()
    {
        const char* const a = "test this";
        const std::string b = "test a ver``y long string that won't fix in small buffer optimization";

        ValueHolder saved;
        {
            Writer writer{ saved };
            serialize( writer, "a", a );
            serialize( writer, "b", b );
        }

        std::string loaded_a;
        const char* loaded_b_str = nullptr; // <- only valid while `saved` exists
        unsigned loaded_b_length = 0;
        {
            Reader reader{ saved };
            serialize( reader, "a", loaded_a );
            reader.serialize( "b", loaded_b_str, loaded_b_length );
        }

        assert( a == loaded_a );
        assert( loaded_b_str != nullptr );
        assert( b == loaded_b_str );
        assert( b.size() == loaded_b_length );
    }
    
    static void test_override()
    {
        ValueHolder value;
        {
            auto sub_obj_serializer = []( ISerializer& serializer )
            {
                serialize( serializer, "i", -32 );
                serialize( serializer, "f", "waaaaaaa" );
                serialize_object( serializer, "nested", []( ISerializer& serializer )
                {
                    serialize( serializer, "blah", "this is an string" );
                } );
                
                serialize( serializer, "i", "test" );
                serialize( serializer, "f", 3.4f );
                serialize( serializer, "nested", -34 );
            };

            Writer writer{ value };
            serialize_object( writer, "a", sub_obj_serializer );
            serialize_object( writer, "b", sub_obj_serializer );
            serialize( writer, "a", true );
        }

        {
            Reader reader{ value };
            bool b = false;
            reader.serialize( "a", b );
            assert( b == true );

            bool called = false;
            serialize_object( reader, "b", [&called]( ISerializer& serializer )
            {
                called = true;
                std::string test;
                float f = 0.f;
                int nested = 0;
                serialize( serializer, "i", test );
                serialize( serializer, "f", f );
                serialize( serializer, "nested", nested );

                assert( test == "test" );
                assert( f = 3.4f );
                assert( nested == -34 );
            } );

            assert( called );
        }
    }

    template < typename T >
    static void test_basic_arrays( const std::vector< T >& array, const T single_element_value )
    {
        ValueHolder value;
        {
            Writer writer{ value };
            serialize( writer, "array", array );
            serialize( writer, "single_element", single_element_value );
        }
        
        std::vector< T > loaded_array;
        std::vector< T > single_element_array;
        {
            Reader reader{ value };
            serialize( reader, "array", loaded_array );
            serialize( reader, "single_element", single_element_array );
        }

        assert( array.size() == loaded_array.size() );
        for( std::size_t i = 0; i < array.size(); ++i )
        {
            assert( array[i] == loaded_array[i] );
        }

        assert( single_element_array.size() == 1 );
        assert( single_element_array[0] == single_element_value );
    }

    static void test_object_arrays()
    {
        const std::vector< std::shared_ptr< Hierarchy > > hierarchies
        {
            build_hierarchy( 4 ),
            build_hierarchy( 13 ),
            build_hierarchy( 6 ),
            nullptr,
            build_hierarchy( 1 ),
            nullptr,
        };

        ValueHolder value;
        {
            Writer writer{ value };
            serialize( writer, "hierarchies", hierarchies );
        }

        std::vector< std::shared_ptr< Hierarchy > > loaded_hierarchies;
        {
            Reader reader{ value };
            serialize( reader, "hierarchies", loaded_hierarchies );
        }

        test_equal_hierarchies(hierarchies, loaded_hierarchies );
    }
    
    static void test_large_array()
    {
        std::vector< int > ints;
        ints.resize( 1453, 0 );
        for ( int i = 0; i < ints.size(); ++i )
        {
            ints[i] = ( i + 1 ) * ( i % 2 == 0 ? 1 : -1 );
        } 

        ValueHolder holder;
        {
            Writer writer{ holder };
            serialize( writer, "array", ints );
        }

        std::vector< int > loaded_ints;
        {
            Reader reader{ holder };
            serialize( reader, "array", loaded_ints );
        }

        assert( loaded_ints.size() == ints.size() );
        for( std::size_t i = 0; i < ints.size(); ++i )
        {
            assert( loaded_ints[i] == ints[i] );
        }
    }

    static void test_save_to_file()
    {
        std::unique_ptr< Hierarchy > root = build_hierarchy( 10 );

        const char* const filename = "test_save_to_file.temp_file";

        {
            ValueHolder value;
            Writer writer{ value };
            serialize( writer, *root );
            save( filename, value );
        }

        Hierarchy loaded_hierarchy;
        {
            ValueHolder value;
            load( filename, value );
            Reader reader{ value };
            serialize( reader, loaded_hierarchy );
        }

        std::filesystem::remove( filename );
        
        assert( *root == loaded_hierarchy );
    }
};

// 
// ConversionTests
// 

// Tests that the conversion from one serializer to an other is the expected one.
// Right now this class performs the test using saved files, 
template < class SrcValueHolder, class DestValueHolder, class DestWriter >
struct ConversionTest
{
    static void test( const char* const src_file, const char* const dest_reference_file, void (*fn)( const SrcValueHolder& holder, ISerializer& writer ) )
    {
        const char* const temp_file = "temp_file.delete_me";
        assert( std::string{ temp_file } != src_file );
        assert( std::string{ temp_file } != dest_reference_file );

        // Convert and save to file
        {
            SrcValueHolder src_holder;
            load( src_file, src_holder );

            DestValueHolder dest_holder;
            DestWriter writer{ dest_holder };
            fn( src_holder, writer );
            
            save( temp_file, dest_holder );
        }

        // Compare the written file with the reference
        bool files_match = true;
        {
            const auto ref_file_size = std::filesystem::file_size( dest_reference_file );
            assert( ref_file_size == std::filesystem::file_size( temp_file ) );

            std::ifstream converted_file{ temp_file };
            std::ifstream reference_file{ dest_reference_file };

            std::unique_ptr< char[] > conv_data = std::make_unique< char[] >( ref_file_size );
            std::unique_ptr< char[] > ref_data = std::make_unique< char[] >( ref_file_size );

            converted_file.read( conv_data.get(), ref_file_size );
            reference_file.read( ref_data.get(), ref_file_size );

            for( std::uintmax_t i = 0; i < ref_file_size; ++i )
            {
                if( conv_data[i] != ref_data[i] )
                {
                    files_match = false;
                    break;
                }
            }
        }

        assert( files_match );
        
        // If some assert fails this file won't be deleted but we are ok with that behavior because we will be able to 
        // open it and see why the test failed.
        std::filesystem::remove( temp_file );
    }
};


// 
// PerformanceTest
// 

struct Timer
{
    Timer() { reset(); }

    float average_time( unsigned divider = 1 ) const
    {
        const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - m_start);
        const auto average_microsec = microseconds.count() / divider;
        return average_microsec / 1000.f;
    }

    const char* units() const { return "ms"; }
    void reset() { m_start = std::chrono::system_clock::now(); }
    
    std::chrono::system_clock::time_point m_start;
};

struct DummySerializerDataHolder {};
class DummySerializer final : public ISerializer
{
public:
    DummySerializer() = default;
    explicit DummySerializer( DummySerializerDataHolder& ) {}   // To keep consistency over other serializers

    bool is_reader() const override { return false; };
    bool has_member( const SerializerString name ) const override { return false; }

    void serialize( const SerializerString name, int& var ) override {}
    void serialize( const SerializerString name, unsigned int& var ) override {}
    void serialize( const SerializerString name, float& var ) override {}
    void serialize( const SerializerString name, bool& var ) override {}
    void serialize( const SerializerString name, const char*& str, unsigned& length ) override {}
    void serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn ) override {}
    void iterate_elements( void* const user_data, const IterateElementsFn fn ) override {}
    
    void serialize( const SerializerString name, const SerializerArray< int >& var ) override {}
    void serialize( const SerializerString name, const SerializerArray< unsigned int >& var ) override {}
    void serialize( const SerializerString name, const SerializerArray< float >& var ) override {}
    void serialize( const SerializerString name, const SerializerArray< bool >& var ) override {}
    void serialize( const SerializerString name, const SerializerArray< SerializerString >& array ) override {}

    void write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn ) override {}
    unsigned read_object_array_size( const SerializerString name ) override { return 0; }
    void read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn ) override {}
};

template < class SrcValueHolder >
struct PerformanceTest
{
    static void run( const char* const src_file, void (*conv_fn)( const SrcValueHolder& holder, ISerializer& writer ) )
    {
        const char* const temp_file = "temp_file.delete_me";
        assert( std::string{ temp_file } != src_file );

        constexpr unsigned iterations = 10;
        Timer timer;

        SrcValueHolder src_holder;

        timer.reset();
        for( unsigned i = 0; i < iterations; ++i )
        {
            load( src_file, src_holder );
        }
        std::cout << "    Load: " << timer.average_time( iterations ) << timer.units() << std::endl;

        timer.reset();
        for( unsigned i = 0; i < iterations; ++i )
        {
            save( temp_file, src_holder );
        }
        std::cout << "    Save: " << timer.average_time( iterations ) << timer.units() << std::endl;
        std::filesystem::remove( temp_file );

        timer.reset();
        for( unsigned i = 0; i < iterations; ++i )
        {
            DummySerializer dummy;
            conv_fn( src_holder, dummy );
        }
        std::cout << "    Iteration: " << timer.average_time( iterations ) << timer.units() << std::endl;
    }
};

// 
// BinarySerializer custom tests
// 

void test_binary_memory_chunks_simple()
{
    BinaryDataHolder holder_c;

    // Save
    {
        // Write into two different objects
        BinaryDataHolder holder_a;
        BinaryDataHolder holder_b;
        {
            BinaryWriter writer{ holder_a };
            serialize( writer, "i", -24 );
            serialize( writer, "b", false );
            serialize( writer, "u", 45 );
        }
        {
            BinaryWriter writer{ holder_b };
            serialize( writer, "f", -30.42f );
            serialize( writer, "s", "this is an string to test binary memory chunks" );
        }
        
        // Write the objects into a third object as binary data
        {
            BinaryWriter writer{ holder_c };
            write_sub_binary_holder( writer, "b", holder_b );
            write_sub_binary_holder( writer, "a", holder_a );
        }
    }

    // Read
    BinaryReader reader{ holder_c };
    {
        const BinaryData data_a = read_sub_binary_holder( reader, "a" );
        BinaryReader reader{ data_a.m_strings, data_a.m_data, data_a.m_data_size };
        
        int i = 0;
        bool b = true;
        unsigned u = 0;
        serialize( reader, "i", i );
        serialize( reader, "b", b );
        serialize( reader, "u", u );
        assert( i == -24 );
        assert( b == false );
        assert( u == 45 );
    }

    {
        const BinaryData data_b = read_sub_binary_holder( reader, "b" );
        BinaryReader reader{ data_b.m_strings, data_b.m_data, data_b.m_data_size };

        float f = 0.f;
        std::string s;
        serialize( reader, "f", f );
        serialize( reader, "s", s );

        assert( f == -30.42f );
        assert( s == "this is an string to test binary memory chunks" );
    }

}

void test_binary_memory_chunks()
{
    const std::vector< std::shared_ptr< Hierarchy > > hierarchy_a
    {
        build_hierarchy( 4 ),
        build_hierarchy( 13 ),
        build_hierarchy( 6 ),
        nullptr,
        build_hierarchy( 1 ),
        nullptr,
    };

    const std::vector< std::shared_ptr< Hierarchy > > hierarchy_b
    {
        build_hierarchy( 20 ),
        nullptr,
        build_hierarchy( 71 ),
        nullptr,
        build_hierarchy( 135 ),
    };

    BinaryDataHolder holder_a;
    BinaryDataHolder holder_b;
    {
        BinaryWriter writer{ holder_a };
        serialize( writer, "blah", hierarchy_a );
    }
    {
        BinaryWriter writer{ holder_b };
        serialize( writer, "blah", hierarchy_b );
    }

    BinaryDataHolder holder_c;
    {
        BinaryWriter writer{ holder_c };
        
        // verbose version
        {
            std::unique_ptr< unsigned char[] > data;
            std::size_t used_size = 0;
            std::size_t data_size = 0;
            save_to_memory( holder_b, data, used_size, data_size );
            
            MemoryChunk chunk;
            chunk.m_data = data.get();
            chunk.m_data_size = used_size;
            writer.write_memory_chunk( "b", chunk );
        }
        
        // helper function version
        write_sub_binary_holder( writer, "a", holder_a );
    }

    std::vector< std::shared_ptr< Hierarchy > > loaded_a;
    std::vector< std::shared_ptr< Hierarchy > > loaded_b;

    BinaryReader reader_c{ holder_c };
    {
        BinaryData data_a;
        
        // verbose version
        {
            const MemoryChunk chunk_a = reader_c.read_memory_chunk( "a" );
            load_from_memory( chunk_a.m_data, chunk_a.m_data_size, data_a );
        }

        BinaryReader reader_a{ data_a.m_strings, data_a.m_data, data_a.m_data_size };
        serialize( reader_a, "blah", loaded_a );
    }

    {
        // helper function version
        const BinaryData data_b = read_sub_binary_holder( reader_c, "b" );

        BinaryReader reader_b{ data_b.m_strings, data_b.m_data, data_b.m_data_size };
        serialize( reader_b, "blah", loaded_b );
    }

    test_equal_hierarchies( hierarchy_a, loaded_a );
    test_equal_hierarchies( hierarchy_b, loaded_b );
}


// 
// Main test functions
// 

#define RUN_TEST( test, ... )   \
    std::cout << #test << std::endl; \
    test( __VA_ARGS__ )

void run_unit_tests()
{
    const std::vector< int > ints{ 4, -3, 2, -56, 23 };
    const int i = -3333;
    const std::vector< unsigned > uints{ 4, 3, 2, 56, 23 };
    const unsigned u = 3333;
    const std::vector< float > floats{ 4.2f, -3.4f, 2.7f, -56.24f, 23.19f };
    const float f = 3333.444f;
    const std::vector< bool > bools{ true, false, false, true, false, true, false, false };
    const bool b = true;
    const std::vector< std::string > strings{ "foo", "blah", "very long string so that it doesn't fit in the sbo (small buffer optimization)", "bar" };
    const std::string s = "this is a single string, not an array of strings";

    // Json tests
    #if 1
    {
        using JsonTests = SerializerTests< JsonReader, JsonWriter, json::value >;
        RUN_TEST( JsonTests::test_serialize_deserialize );
        RUN_TEST( JsonTests::test_try_to_load_non_existent_variable );
        RUN_TEST( JsonTests::test_empty_elements_are_not_saved );
        RUN_TEST( JsonTests::test_value_conversion );
        RUN_TEST( JsonTests::test_value_conversion_global_functions );
        RUN_TEST( JsonTests::test_hiearchy );
        RUN_TEST( JsonTests::test_iterate_elements );
        RUN_TEST( JsonTests::test_iterate_elements_in_writer_and_with_objects );
        RUN_TEST( JsonTests::test_strings );
        RUN_TEST( JsonTests::test_override );
        RUN_TEST( JsonTests::test_save_to_file );
        RUN_TEST( JsonTests::test_basic_arrays< int >, ints, i );
        RUN_TEST( JsonTests::test_basic_arrays< unsigned >, uints, u );
        RUN_TEST( JsonTests::test_basic_arrays< float >, floats, f );
        RUN_TEST( JsonTests::test_basic_arrays< bool >, bools, b );
        RUN_TEST( JsonTests::test_basic_arrays< std::string >, strings, s );
        RUN_TEST( JsonTests::test_large_array );
        RUN_TEST( JsonTests::test_object_arrays );
    }
    #endif
    
    // Binary tests
    #if 1
    {
        using BinaryTests = SerializerTests< BinaryReader, BinaryWriter, BinaryDataHolder >;
        RUN_TEST( BinaryTests::test_serialize_deserialize );
        RUN_TEST( BinaryTests::test_try_to_load_non_existent_variable );
        RUN_TEST( BinaryTests::test_empty_elements_are_not_saved );
        RUN_TEST( BinaryTests::test_value_conversion );
        RUN_TEST( BinaryTests::test_value_conversion_global_functions );
        RUN_TEST( BinaryTests::test_hiearchy );
        RUN_TEST( BinaryTests::test_iterate_elements );
        RUN_TEST( BinaryTests::test_iterate_elements_in_writer_and_with_objects );
        RUN_TEST( BinaryTests::test_strings );
        RUN_TEST( BinaryTests::test_override );
        RUN_TEST( BinaryTests::test_save_to_file );
        RUN_TEST( BinaryTests::test_basic_arrays< int >, ints, i );
        RUN_TEST( BinaryTests::test_basic_arrays< unsigned >, uints, u );
        RUN_TEST( BinaryTests::test_basic_arrays< float >, floats, f );
        RUN_TEST( BinaryTests::test_basic_arrays< bool >, bools, b );
        RUN_TEST( BinaryTests::test_basic_arrays< std::string >, strings, s );
        RUN_TEST( BinaryTests::test_large_array );
        RUN_TEST( BinaryTests::test_object_arrays );

        RUN_TEST( test_binary_memory_chunks_simple );
        RUN_TEST( test_binary_memory_chunks );
    }
    #endif
    
    // Conversion tests
    #if 1
    {
        using JsonToBinaryConversion = ConversionTest< json::value, BinaryDataHolder, BinaryWriter >;
        RUN_TEST( JsonToBinaryConversion::test, "large.json", "large.bnr", json_to_other );
        
        using BinaryToJsonConversion = ConversionTest< BinaryDataHolder, json::value, JsonWriter >;
        RUN_TEST( BinaryToJsonConversion::test, "large.bnr", "large.json", binary_to_other );
    }
    #endif
}

void run_performance_tests()
{
    #if 1
    {
        using JsonPerformanceTest = PerformanceTest< json::value >;
        RUN_TEST( JsonPerformanceTest::run, "large.json", json_to_other );

        using BinaryPerformanceTest = PerformanceTest< BinaryDataHolder >;
        RUN_TEST( BinaryPerformanceTest::run, "large.bnr", binary_to_other );
    }
    #endif
}

int main( const int argc, const char** argv )
{
    Timer timer;

    if( argc == 1 )
    {
        run_unit_tests();
        run_performance_tests();
    }
    else
    {
        if( argv[1] == std::string{ "-unit-tests" } )
        {
            run_unit_tests();
        }
        else if( argv[1] == std::string{ "-perf-tests" } )
        {
            run_performance_tests();
        }
    }
    
    std::cout << "Tests executed in " << timer.average_time() << timer.units() << std::endl;
    return 0;
}
