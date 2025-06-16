
#pragma once

#include <string>
#include <vector>

extern std::vector<std::string> __names;
extern std::vector<std::string> __shortNames;

namespace gamemodes
{

enum GameMode
{
    // These are the supported game modes. A supported game mode usually has a few
    // specific settings. Unsupported game modes will work properly.
    Blitz = 0,
    YurisRevenge = 1,
    RedAlert2 = 2,
    RedAlert = 3,
    RedAlert2NewMaps = 4,
    Blitz2v2 = 5,

    GameModeCount = 6,
    Unknown = 99
};

// Get the number of game modes.
inline constexpr int count()
{
    return static_cast<int>(GameModeCount);
}

std::string name(GameMode gameMode);
std::string shortName(GameMode gameMode);

//! Get a list of all game modes.
std::vector<GameMode> list();
uint32_t playerCount(GameMode gameMode);

inline constexpr GameMode toGameMode(int i)
{
    return static_cast<GameMode>(i);
}

//! Get the game type from a string.
GameMode toGameMode(const std::string &name);

//! Specific decay factor for each game mode.
double decayFactor(GameMode gameMode);

//! Deviation threshold to become active.
double deviationThresholdActive(GameMode gameMode, double elo);

//! Deviation threshold to become inactive.
double deviationThresholdInactive(GameMode gameMode, double elo);

//! Maximum deviation you can get after having been active is game-dependent.
double maxDeviationAfterActive(GameMode gameMode);

} // namespace gamemodes
