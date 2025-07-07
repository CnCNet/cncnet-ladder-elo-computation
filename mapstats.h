 
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <set>

#include "rating.h"
#include "faction.h"
#include "game.h"
#include "probabilities.h"

struct MapPlayed
{
    std::string mapName;
    uint32_t count = 0;
    std::set<uint32_t> differentPlayers;
    bool operator<(const MapPlayed &other) const { return (count != other.count) ? count > other.count : mapName > other.mapName; }
};

struct Upset
{
    std::chrono::year_month_day date;
    std::vector<uint32_t> winners;
    std::vector<uint32_t> losers;
    std::string map;
    std::vector<factions::Faction> winnerFactions;
    std::vector<factions::Faction> loserFactions;
    std::vector<int> winnerElo;
    std::vector<int> loserElo;
    double eloDifference;
    uint32_t duration = 0;

    // Need the biggest difference on top.
    bool operator<(const Upset &other) const { return eloDifference > other.eloDifference; }

    factions::Faction winnerFaction() const { return faction(winnerFactions); }
    factions::Faction loserFaction() const { return faction(loserFactions); }

private:
    factions::Faction faction(const std::vector<factions::Faction> &factions) const
    {
        if (std::all_of(factions.begin(), factions.end(),
            [&](factions::Faction faction){ return faction == factions::Soviet; }))
        {
            return factions::Soviet;
        }
        else if (std::all_of(factions.begin(), factions.end(),
            [&](factions::Faction faction){ return faction == factions::Allied; }))
        {
            return factions::Allied;
        }
        else if (std::all_of(factions.begin(), factions.end(),
            [&](factions::Faction faction){ return faction == factions::Yuri; }))
        {
            return factions::Yuri;
        }

        return factions::Combined;
    }
};

struct Team
{
    uint64_t teamId;
    uint32_t games = 0;
    uint32_t wins;
    double teamELO;
    double eloDifference;
    std::chrono::year_month_day lastGame;

    uint32_t player1() const { return static_cast<uint32_t>(teamId & 0xFFFFFFFF); }
    uint32_t player2() const { return static_cast<uint32_t>(teamId >> 32); }

    bool operator<(const Team &other) const { return eloDifference > other.eloDifference; }
};

class MapStats
{
public:
    //! Constructor.
    MapStats(gamemodes::GameMode gameMode);

    //! Destructor.
    ~MapStats() = default;

    //! Finalize map statistics.
    void finalize(const std::filesystem::path &directory, const Players &players, std::chrono::sys_days date);

    //! Add a game to the stats.
    void processGame(const Game &game, const Players &players);

    //! Export alltime, yearly and monthly upsets.
    void exportUpsets(const std::filesystem::path &directory, const Players &players);

    //! Export the longest games.
    void exportLongestGames(const std::filesystem::path &directory, const Players &player);

    //! Export the maps played for each month.
    void exportMapsPlayed(const std::filesystem::path &directory);

    //! Export best teams for Blitz2v2.
    void exportBestTeams(const std::filesystem::path &directory, const Players &players);

private:
    //! Export the given list of upsets.
    void exportUpsets(const std::filesystem::path &directory, const std::multiset<Upset> &upsets, const std::string &filename, const std::string &description, const Players &players) const;

    //! Game mode used.
    gamemodes::GameMode _gameMode;

    //! Number of games passed to this class.
    int _gameCount = 0;

    //! Overall map statistics each map.
    std::map<std::string, Probabilities> _mapStats[factions::UnknownSetup];

    //! Teams statistics for blitz-2v2.
    std::map<uint64_t, Probabilities> _teamStats;
    std::map<uint64_t, std::vector<std::pair<double, double>>> _lastTeamELOs;

    //! Teams statistics;
    std::multiset<Team> _teams;

    //! Teams statistics for the day before.
    std::multiset<Team> _yesterdaysTeams;

    //! Number of games for each player on each for each month.
    //! Key of second map is the normalized map name.
    std::map<std::chrono::year_month_day, std::map<std::string, MapPlayed>> _gameCountsPerMonthAndPlayer;

    //! Average duration of a game. First value of the pair is the sum of durations,
    //! second value is the number of games. Key to the map is the map name.
    std::map<std::string, std::pair<uint32_t, uint32_t>> _averageDuration;

    //! Upsets per month.
    std::map<std::chrono::year_month_day, std::multiset<Upset>> _upsetsMonthly;

    //! Upsets per month.
    std::multiset<Upset> _upsetsLast12Month;

    //! Upsets last 30 days.
    std::multiset<Upset> _upsetsLast30Days;

    //! Upsets per year.
    std::multiset<Upset> _upsetsAllTime;

    //! Longest ranked match games.
    std::multiset<Upset, std::function<bool (const Upset&, const Upset&)>> _longestGames;

}; // class MapStats

