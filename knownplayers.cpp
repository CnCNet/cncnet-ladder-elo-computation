
#include <algorithm>
#include <array>

#include "knownplayers.h"

namespace players
{

constexpr std::array<KnownPlayers, 6> proPlayers = {
    KnownPlayers::Luke, KnownPlayers::Marko, KnownPlayers::Latof, KnownPlayers::Qien, KnownPlayers::Root,
    KnownPlayers::Kwos
};

bool isProPlayer(uint32_t id)
{
    return std::find(proPlayers.begin(), proPlayers.end(), static_cast<KnownPlayers>(id)) != proPlayers.end();
}

}
