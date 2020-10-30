#ifndef CONFIG_HH_INCLUDE_GUARD
#define CONFIG_HH_INCLUDE_GUARD

#include <string>
#include <vector>
#include <map>

#ifdef JSON_DLL_BUILD
#define JSON_API __declspec(dllexport)
#elif defined( JSON_DLL )
#define JSON_API __declspec(dllimport)
#else
#define JSON_API
#endif

namespace json
{

	//! Allocator type used by all the json functions and classes
	template <typename T>
	using allocator = std::allocator<T>;

	//! String type used by all the json functions and classes
	using string = std::basic_string<char, std::char_traits<char>, json::allocator<char>>;

	//! Vector type used by all the json functions and classes
	template <typename T>
	using vector = std::vector<T, json::allocator<T>>;

	//! Map type used by all the json functions and classes
	template <typename Key, typename Value>
	using map = std::map<Key, Value, std::less<>, json::allocator<std::pair<const Key, Value>>>;

}

#endif // CONFIG_HH_INCLUDE_GUARD

