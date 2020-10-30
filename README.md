# ISerializer

This project was born as a possible serialization interface for a game, trying to solve some of the problems I faced while working on games with a custom engine.
The problems are:
  - Having to implement two functions per type that needs to be loaded/saved.
  - Ability to load/save data in different formats (i.e. text format during development and binary for the final version).

## Initial Goals
This serialization interface is designed with these goals in mind:
  - Programmer Friendly:
    - Avoid having to implement two functions to load/save, instead, we can just implement one `serialize( ISerialize& s, ... )` function that will do both.
    - Easy to extend interface by the user to support serialization for its own types.
    - Easy to serialize the data in different formats (i.e. json, xml, binary).
  - Performance
    - Keep as much performance as possible while loading/saving (i.e. load binary arrays using `memcpy` instead of loading per element).
    - Ability to use the same Serializer for loading data concurrently in different threads (`Reader`s must be stateless).

## Implementation

Main interface files:
  - `serializer.h`: Main interface of the serialization interface
  - `serializer_config.h`: User can modify this file to configure how the serializer should be compiled.

Serializer specializations:
  - `json_serializer.h`
  - `json_serializer.cpp`
  - `binary_serializer.h`
  - `binary_serializer.cpp`

Extensions (optional files):
  - `serializer_ext.h`: Extends the serializer global interface with some helpers.
  - `serializer_std.h`: Extends the serializer global interface with support for some types of the stl.
  - `serializer_std.cpp`: 

### Interface

We use the same inerface to load and save data, `ISerializer`. This way the user only needs to implement one function to load/save it's code.

For each format type we need to implement two specializations of `ISerializer`: `Writer` and `Reader`.
  - `Writer`: Writes the variables into that format (i.e. `JsonWriter` writes the variables to a json format).
  - `Reader`: Reads the data from that format into variables (i.e. `JsonReader`).

`ISerializer` functions take references to variables, if the specialization is a `Reader` the serializer will change it's value; if is a `Writer` it won't change it.
This is one of the caveats of this design, we cannot use `const` objects or variables when saving our data.

### User code

Instead of using directly the `ISerializer` functions, the user should use the global versions of the functions. This way extending the interface is as simple as adding an overload to the `serialize( ... )` function.

Examples: extending the interface of the serializer, all this code is written by the user.
```cpp
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

// Some function that saves the game state
void save_game( ISerializer& s )
{
    // Hypotetical values we want to save
    int collectibles_picked = 10;
    int death_count = 3;
    PlayerStats player_stats;

    // Actual serialization code
    serialize( s, "collectibles", collectibles_picked );
    serialize( s, "deaths", death_count );
    serialize( s, "player", player_stats ); // <- Uses the same interface as built in types.
}
```

Check `examples.cpp` for more examples.

## Specializations

One of the goals of this interface is to be able to serialize data in different formats, that's why I decided to implement a text (json) and binary (custom) formats.

### Json Serializer

I used the json C++ implementation by [asielorz][asielorz-github] at [asielorz/Json][asielorz-json].
The wrapper for this json interface can be found in `json_serializer.h` and `json_serializer.cpp`.

[asielorz-github]: https://github.com/asielorz
[asielorz-json]: https://github.com/asielorz/Json

### Binary Serializer

The binary serializer serializes the data in a custom binary format.
It uses a string table to reduce the size of the file in disk and being able to load it fast.
When loading the file, it only needs to "parse" is the string table, the rest of the data is loaded as is and the Reader can work with it right the way.

### Performance Tests

This is the output from running the performance tests on the json wrapper of the serializer and the binary serializer.
We can see how loading and saving of the binary version is faster, because we don't need to convert the data to an string representation.

The iteration is also a bit faster because we are iterating over contiguous memory.

```
JsonPerformanceTest::run
    Load: 36.279ms
    Save: 33.137ms
    Iteration: 0.002ms
BinaryPerformanceTest::run
    Load: 0.913ms
    Save: 8.387ms
    Iteration: 0ms
```

## Possible additions

This is just the very basic implementation of the serializer, there is still room for improvement.
Some of them are:
  - Add a hash to `SerializerString` so that comparing for key/values is faster by some serializers.
  - Add versioning to `BinaryDataHolder` so that we can handle format changes.
  - Add more methods to `ISerializer` to allow the user to make some decisions depending on the data (i.e. provide a `get_type` function that will let the user know the type of an element before loading it).

There are comments with the format `// #` in the code with more ideas/improvements.

