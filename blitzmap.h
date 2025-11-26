
#pragma once

#include <array>
#include <string>

namespace blitzmap
{

enum MapSize
{
    Small = 0,
    Medium = 1,
    Large = 2
};

inline constexpr int MapCount = 47;

extern std::array<std::string, MapCount> __names;
extern std::array<std::string, MapCount> __shortNames;
extern std::array<MapSize, MapCount> __mapSizes;

//! Get the index to a map name. Return -1 if map could not be found.
int toIndex(const std::string &name);

//! Get the number of maps.
constexpr std::size_t count()
{
    return __names.size();
}

} // namespace blitzmap

