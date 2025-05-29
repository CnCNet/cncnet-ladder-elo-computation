
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

extern std::array<std::string, 45> __names;
extern std::array<std::string, 45> __shortNames;
extern std::array<MapSize, 45> __mapSizes;

//! Get the index to a map name. Return -1 if map could not be found.
int toIndex(const std::string &name);

//! Get the number of maps.
constexpr std::size_t count()
{
    return __names.size();
}

} // namespace blitzmap

