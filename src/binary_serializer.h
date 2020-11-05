#pragma once

#include "serializer.h"

#include <string>
#include <vector>
#include <memory>

// "Root" object: Owns the data loaded from a file or generated with a BinaryWriter.
struct SERIALIZER_API BinaryDataHolder
{
    // #todo_borja Add data versioning and load/save it to file for backwards compatibility.
    // #study_borja Consider storing an array of chars and then a separate array holding the lengths.
    //              This way we only make one big memory allocation for the strings instead of one for the vector and others for the strings. 
    //              When saving to a file we could store both arrays or only the one with the strings (as the lengths can be found by checking the strings).
    std::vector< std::string > m_strings;
    std::unique_ptr< unsigned char[] > m_data;
    std::size_t m_data_size{ 0 };
    std::size_t m_used_size{ 0 };
};

struct SERIALIZER_API MemoryChunk
{
    const unsigned char* m_data{ nullptr };
    unsigned m_data_size{ 0 };
};

// Similar to `BinaryDataHolder` but doesn't own the memory.
struct SERIALIZER_API BinaryData
{
    std::vector< std::string > m_strings;
    const unsigned char* m_data{ nullptr };
    unsigned m_data_size{ 0 };
};

SERIALIZER_API void save_to_memory( const BinaryDataHolder& data, std::unique_ptr< unsigned char[] >& out_data, std::size_t& out_used_size, std::size_t& out_data_size );
SERIALIZER_API void load_from_memory( const unsigned char* data, const unsigned data_size, BinaryData& out_data );

#ifdef SERIALIZER_ENABLE_IO_FUNCTIONS
#include <ostream>
#include <istream>
SERIALIZER_API void save( std::ostream& os, const BinaryDataHolder& holder );
SERIALIZER_API void load( std::istream& os, BinaryDataHolder& holder );
SERIALIZER_API void save( const char* filename, const BinaryDataHolder& holder );
SERIALIZER_API void load( const char* filename, BinaryDataHolder& holder );
#endif

class SERIALIZER_API BinaryWriter final : public ISerializer
{
public:
    explicit BinaryWriter( BinaryDataHolder& holder )
        : BinaryWriter{ holder.m_strings, holder.m_data, holder.m_data_size, holder.m_used_size }
    {}
    BinaryWriter( 
        std::vector< std::string >& strings,
        std::unique_ptr< unsigned char[] >& data,
        std::size_t& data_size,
        std::size_t& used_size )
        : m_first_element_header_start{ used_size }
        , m_strings{ strings }
        , m_data{ data }
        , m_data_size{ data_size }
        , m_used_size{ used_size }
    {}

    ~BinaryWriter();
    
    void write_memory_chunk( const SerializerString name, MemoryChunk chunk ) const;

    // ISerializer
    bool is_reader() const override;
    bool has_member( const SerializerString name ) const override;

    void serialize( const SerializerString name, int& var ) override;
    void serialize( const SerializerString name, unsigned int& var ) override;
    void serialize( const SerializerString name, float& var ) override;
    void serialize( const SerializerString name, bool& var ) override;
    void serialize( const SerializerString name, const char*& str, unsigned& length ) override;
    void serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn ) override;
    void iterate_elements( void* const user_data, const IterateElementsFn fn ) override;
    
    void serialize( const SerializerString name, const SerializerArray< int >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< unsigned int >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< float >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< bool >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< SerializerString >& array ) override;

    void write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn ) override;
    unsigned read_object_array_size( const SerializerString name ) override;
    void read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn ) override;

private:
    const std::size_t m_first_element_header_start{ 0 };
    std::vector< std::string >& m_strings;
    std::unique_ptr< unsigned char[] >& m_data;
    std::size_t& m_data_size;
    std::size_t& m_used_size;
};

class SERIALIZER_API BinaryReader final : public ISerializer
{
public:
    explicit BinaryReader( const BinaryDataHolder& holder )
        : m_strings{ holder.m_strings }
        , m_data{ holder.m_data.get() }
        , m_data_size{ holder.m_used_size }
    {}
    explicit BinaryReader( 
        const std::vector< std::string >& strings,
        const unsigned char* const data,
        const std::size_t data_size )
        : m_strings{ strings }
        , m_data{ data }
        , m_data_size{ data_size }
    {}

    MemoryChunk read_memory_chunk( const SerializerString name ) const;

    // ISerializer
    bool is_reader() const override;
    bool has_member( const SerializerString name ) const override;

    void serialize( const SerializerString name, int& var ) override;
    void serialize( const SerializerString name, unsigned int& var ) override;
    void serialize( const SerializerString name, float& var ) override;
    void serialize( const SerializerString name, bool& var ) override;
    void serialize( const SerializerString name, const char*& str, unsigned& length ) override;
    void serialize( const SerializerString name, void* const user_data, const SerializeObjFn fn ) override;
    void iterate_elements( void* const user_data, const IterateElementsFn fn ) override;

    void serialize( const SerializerString name, const SerializerArray< int >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< unsigned int >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< float >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< bool >& var ) override;
    void serialize( const SerializerString name, const SerializerArray< SerializerString >& array ) override;
    
    void write_object_array( const SerializerString name, unsigned element_num, void* user_data, SerializeObjArrayFn fn ) override;
    unsigned read_object_array_size( const SerializerString name ) override;
    void read_object_array( const SerializerString name, void* user_data, SerializeObjArrayFn fn ) override;
    
private:
    const std::vector< std::string >& m_strings;
    const unsigned char* const m_data{ nullptr };
    const std::size_t m_data_size{ 0 };
};

// Helper functions to store the data from a BinaryDataHolder as a sub element of an other element.
SERIALIZER_API void write_sub_binary_holder( BinaryWriter& writer, const SerializerString name, const BinaryDataHolder& holder );
SERIALIZER_API BinaryData read_sub_binary_holder( BinaryReader& reader, const SerializerString name );

// Data conversion
SERIALIZER_API void binary_to_other( const BinaryDataHolder& data_holder, ISerializer& writer );