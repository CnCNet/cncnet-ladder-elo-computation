
#pragma once

#include <cstdint>

// A couple of active players used for testing purposes.
enum class KnownPlayers : uint32_t
{
    Marsh = 6,
    Zedd = 13,
    Luke = 152,
    Ludo = 153,
    Burg = 332,
    Marko = 928,
    Edd = 2152,
    Latof = 3118,
    Andy = 6026,
    Muri = 7922,
    Lloyd = 17221,
    Qien = 17651,
    Mueller = 24830,
    Snark = 36141,
    Kain = 39603,
    Ossama = 41776,
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

// Some pro players for extended stats.
enum class ProPlayers : uint32_t
{
    Luke = 152,
    Marko = 928,
    Latof = 3118,
    Qien = 17651,
    Root = 48373,
    Kwos = 54423
};

namespace players
{

extern bool isProPlayer(uint32_t id);

}
