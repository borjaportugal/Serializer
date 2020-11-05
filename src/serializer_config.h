// This file can be modified by the user to easily compile the serializer in the way it wants.

#pragma once

// DLL compilation
#ifndef SERIALIZER_API
#ifdef SERIALIZER_DLL_BUILD
    #define SERIALIZER_API __declspec(dllexport)
#elif defined( SERIALIZER_DLL )
    #define SERIALIZER_API __declspec(dllimport)
#else
    #define SERIALIZER_API
#endif
#endif  // SERIALIZER_API

// Assertion
#ifndef SERIALIZER_ASSERT
#include <assert.h>
#define SERIALIZER_ASSERT( b, str, ... ) assert( b && str #__VA_ARGS__ )
#endif  // SERIALIZER_ASSERT

// IO
#define SERIALIZER_ENABLE_IO_FUNCTIONS
