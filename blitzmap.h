
#pragma once

#include <string>
#include <vector>

namespace blitzmap
{

enum MapSize
{
    Small = 0,
    Medium = 1,
    Large = 2
};

extern std::vector<std::string> __names;
extern std::vector<std::string> __shortNames;
extern std::vector<MapSize> __mapSizes;

//! Get the index to a map name. Return -1 if map could not be found.
int toIndex(const std::string &name);

//! Get the number of maps.
int count();

} // namespace blitzmap

