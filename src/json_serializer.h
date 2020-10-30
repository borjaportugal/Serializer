
#pragma once

#include "serializer.h"

namespace json
{
    class value;
}

class SERIALIZER_API JsonWriter final : public ISerializer
{
public:
    explicit JsonWriter( json::value& value ) : m_value{ value } {}

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
    json::value& m_value;
};

class SERIALIZER_API JsonReader final : public ISerializer
{
public:
    explicit JsonReader( const json::value& value ) : m_value{ value } {}

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
    const json::value& m_value;
};

SERIALIZER_API void json_to_other( const json::value& json_value, const SerializerString member_name, ISerializer& writer );
SERIALIZER_API void json_to_other( const json::value& json_value, ISerializer& writer );
