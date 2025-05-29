 
#pragma once

#include <string>
#include <vector>

namespace gametypes
{

extern std::vector<std::string> __names;
extern std::vector<std::string> __shortNames;

enum GameType
{
    Unknown = 0,
    Quickmatch = 1,
    Showmatch = 2,
    WorldSeries = 3,
};

// Get the number of game modes.
inline constexpr int count()
{
    return 3;
}

std::string name(GameType gameType);
std::string shortName(GameType gameType);

inline constexpr GameType toGameType(int i)
{
    return static_cast<GameType>(i);
}

//! Get the index to a game type. Return -1 if game type could not be found.
int toIndex(const std::string &name);

//! Get the game type from a string.
GameType toGameType(const std::string &name);

} // namespace gametypes
