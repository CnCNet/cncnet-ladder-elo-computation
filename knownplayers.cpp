
#include <algorithm>
#include <array>

#include "knownplayers.h"
#include "rating.h"

namespace players
{

constexpr std::array<KnownPlayers, 6> proPlayers = {
    KnownPlayers::Marko, KnownPlayers::Latof, KnownPlayers::Qien
};

bool isProPlayer(uint32_t id)
{
    return std::find(proPlayers.begin(), proPlayers.end(), static_cast<KnownPlayers>(id)) != proPlayers.end();
}

std::pair<double, double> initialRatingAndDeviation(uint32_t userId, gamemodes::GameMode gameMode)
{
    double initialRating = glicko::initialRating;
    double initialDeviation = glicko::initialDeviation;

    auto contains = [](auto&& container, auto&& value) {
        return std::find(std::begin(container), std::end(container), value) != std::end(container);
    };

    // Assigned some starting values to a couple of players. These values WILL NOT
    // change the final result in terms of gaps, but just adjust the rating of
    // everyone. If changes to the ELO systems are made, we want players to keep
    // their rating, because it might look strange if the rating changes without
    // having played. Several changes have been made over time and these values
    // will just keep the ratings stable. Again: this is not a rating boost or
    // downgrade. Because of the high initial deviation, it has no effect on the
    // rating of a players after 20-30 games.

    if (gameMode == gamemodes::Blitz)
    {
        if (contains(std::initializer_list<KnownPlayers>{ KnownPlayers::Latof, KnownPlayers::Qien, KnownPlayers::Marko, KnownPlayers::Kwos }, static_cast<KnownPlayers>(userId)))
        {
            initialRating = 1850.0;
            initialDeviation = 250.0;
        }

        if (contains(std::initializer_list<KnownPlayers>{ KnownPlayers::Ardee, KnownPlayers::Edd, KnownPlayers::Root, KnownPlayers::Luke }, static_cast<KnownPlayers>(userId)))
        {
            initialRating = 1700.0;
            initialDeviation = 250.0;
        }

        if (contains(std::initializer_list<KnownPlayers>{ KnownPlayers::Iver, KnownPlayers::Sneer, KnownPlayers::Diego }, static_cast<KnownPlayers>(userId)))
        {
            initialRating = 1150.0;
            initialDeviation = 250.0;
        }

        if (contains(std::initializer_list<KnownPlayers>{ KnownPlayers::BlitzBot }, static_cast<KnownPlayers>(userId)))
        {
            initialRating = 500.0;
            initialDeviation = 200.0;
        }
    }
    else if (gameMode == gamemodes::RedAlert2)
    {
        if (contains(std::initializer_list<KnownPlayers>{ KnownPlayers::Latof, KnownPlayers::Qien, KnownPlayers::Marko, KnownPlayers::Kwos, KnownPlayers::Lgnd }, static_cast<KnownPlayers>(userId)))
        {
            initialRating = 1650.0;
            initialDeviation = 300.0;
        }

        if (contains(std::initializer_list<KnownPlayers>{ KnownPlayers::Mueller }, static_cast<KnownPlayers>(userId)))
        {
            initialRating = 800.0;
            initialDeviation = 250.0;
        }
    }
    else
    {
        if (contains(std::initializer_list<KnownPlayers>{ KnownPlayers::Mueller }, static_cast<KnownPlayers>(userId)))
        {
            initialRating = 800.0;
            initialDeviation = 250.0;
        }
    }

    return { initialRating, initialDeviation };
}

}
