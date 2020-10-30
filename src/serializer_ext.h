// This file contains specializations of some Serializer classes, to be able to include them only where needed.

#pragma once

#include "serializer.h"

template < typename T >
struct RawArraySerializer : SerializerArray< T >
{
    RawArraySerializer(const T* get_data, T* set_data, unsigned size, unsigned* const loaded_size = nullptr )
        : m_get_data{ get_data }
        , m_set_data{ set_data }
        , m_array_size{ size }
        , m_loaded_size{ loaded_size }
    {
        SERIALIZER_ASSERT( m_get_data || m_set_data, "Expecting at least one operation." );
    }

    // 
    virtual unsigned get_size() const { return m_array_size; }
    virtual void get_element(unsigned i, T& t) const { t = m_get_data[i]; };

    // 
    virtual void set_size(unsigned size) const 
    { 
        SERIALIZER_ASSERT( size <= m_array_size, "Expecting the same size or smaller.");
        if( m_loaded_size ) *m_loaded_size = size;
    }
    virtual void set_element(unsigned i, T t) const
    {
        SERIALIZER_ASSERT(i < m_array_size, "Invalid index.");
        m_set_data[i] = t;
    }

    // 
    virtual bool supports_get_set_all() const { return true; }
    virtual const T* get_all() const { return m_get_data; };
    virtual void set_all(unsigned size, const T* const data) const
    { 
        SERIALIZER_ASSERT(size <= m_array_size, "Expecting same size or smaller.");
        if( m_loaded_size ) *m_loaded_size = size;
        std::memcpy(m_set_data, data, size * sizeof( T ) );
    }

    const T* const m_get_data = nullptr;
    T* const m_set_data = nullptr;
    const unsigned m_array_size = 0;
    
    unsigned* const m_loaded_size = nullptr;
};