
#pragma once

#include <cstdint>
#include <utility>

#include "gamemode.h"

// A couple of active players used for testing purposes.
enum class KnownPlayers : uint32_t
{
    Luke = 152,
    Marko = 928,
    Edd = 2152,
    Latof = 3118,
    Lloyd = 17221,
    Qien = 17651,
    Mueller = 24830,
    Lgnd = 35501,
    Snark = 36141,
    Diego = 42083,
    Root = 48373,
    Ardee = 51203,
    Iver = 53431,
    Kwos = 54423,
    Gator = 55403,
    Sneer = 58860,
    FourLights = 58868,
    Ziggy = 60864,
    BlitzBot = 64304
};

// Player which caused the ratings to mess up in the past.
enum class CriticalPlayers : uint32_t
{
    Buffalo = 21,
    Kain = 39603,
    GemZKing = 70820,
};

namespace players
{

extern bool isProPlayer(uint32_t id);
extern std::pair<double, double> initialRatingAndDeviation(uint32_t id, gamemodes::GameMode gameMode);

}
