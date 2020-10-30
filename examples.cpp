// This file contains some examples about how to use this serialization interface.
// Examples might use code of previous examples.

#include "serializer.h"
#include "serializer_std.h" // to_ss

#include "binary_serializer.h"
#include "json_serializer.h"

#include "asielorz-json/value.hh"

#include <map>

// Example 1: Extending the serialization interface for a new user type.

struct PlayerStats
{
    int level;
    float health;
};

// Serializes the object inline
void serialize( ISerializer& s, PlayerStats& player_stats )
{
    serialize( s, "level", player_stats.level );
    serialize( s, "health", player_stats.health );
}

// Serializes the object as a sub element
void serialize( ISerializer& s, const SerializerString name, PlayerStats& player_stats )
{
    serialize_object( s, name, [&player_stats]( ISerializer& sub_serializer )
    {
        serialize( sub_serializer, player_stats );
    } );
}

void some_user_function_that_wants_to_save_player_stats( ISerializer& s )
{
    PlayerStats stats;  // assuming is filled

    // We can serialize directly the variables as part of current element
    serialize( s, stats );

    // Or as a sub element
    serialize( s, "player_stats", stats );
}

// Example 2: Saving and loading the world state using the same `serialize` function.
struct GameWorld
{
    // Hypotetical values
    float elapsed_time = 25.3f;
    int collectibles_picked = 10;
    int death_count = 3;
    PlayerStats player_stats;
};

void serialize( ISerializer& s, GameWorld& world )
{
    serialize( s, "collectibles", world.collectibles_picked );
    serialize( s, "deaths", world.death_count );
    serialize( s, "played_time", world.elapsed_time );
    serialize( s, "player", world.player_stats );   // <- Uses the same interface as built in types.
}

// In this case we want to serialize to json (because we want to debug the serialized data)
void save_game( GameWorld& world )
{
    json::value json_value;
    JsonWriter writer{ json_value };
    serialize( writer, world );

    // Then we save to file...
    //save( "game_save.json", json_value );
}
void load_game( GameWorld& world )
{
    json::value json_value;
    // We load from a file...
    //load( "game_save.json", json_value );

    // Then a reader will set the proper values to our variables.
    JsonReader reader{ json_value };
    serialize( reader, world );
}

// Example 3: Generic implementation for loading/saving contents to an std::map
template < typename T >
void serialize( ISerializer& s, std::map< std::string, T >& map )
{
    if( s.is_reader() )
    {
        iterate_elements( s, [&map]( ISerializer& serializer, const SerializerString name )
        {
            T& new_element = map[ to_std_string( name ) ];
            serialize( serializer, name, new_element ); // `serialize` must be overloaded for type T
        } );
    }
    else
    {
        for( const auto it : map )
        {
            T& value = it->second;
            const SerializerString name = to_ss( it->firsts );
            serialize_object( s, name, [&value]( ISerializer& inner_serializer )
            {
                serialize( inner_serializer, value );
            } );s
        }
    }
}

void save_player_stats()
{
    // In a multiplayer game we have several players, save the stats of all of them
    // We assume this map is filled with the data we want per player
    std::map< std::string, PlayerStats > player_stats;

    BinaryDataHolder data;
    BinaryWriter writer{ data };
    serialize( writer, player_stats );
}



// Example 4: Conversion from one format to an other.
void conversion()
{
    // As the `ISerializer` interface wraps the serialization code, we can convert between formats by implementing just one function.
    // In this case we will convert from json to binary and xml

    json::value source_data;    // This json contains a lot of data to be converted.

    // Convert to Binary
    {
        BinaryDataHolder binary_data;
        BinaryWriter writer{ binary_data };
        json_to_other( source_data, writer );

        // At this point binary_data is populated with the same contents as source_data
    }
    
    // Convert to Xml (assuming we implemented XmlObject and XmlWriter)
    {
#if 0   // <- otherwise this file won't compile

        XmlObject oxm_object;
        XmlWriter writer{ oxm_object };
        json_to_other( source_data, writer );
#endif

        // At this point oxm_object is populated with the same contents as source_data
    }
}

