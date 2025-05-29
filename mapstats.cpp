
#include <algorithm>
#include <fstream>
#include <regex>

#include "blitzmap.h"
#include "cplusplus.h"
#include "knownplayers.h"
#include "players.h"
#include "logging.h"
#include "mapstats.h"
#include "stringtools.h"

// TODO:
// - Most games a day/month/year

/*!
 */
MapStats::MapStats(gamemodes::GameMode gameMode) :
    _gameMode(gameMode),
    _longestGames([](const Upset &a, const Upset &b) { return a.duration > b.duration; })
{
}

/*!
 */
void MapStats::processGame(const Game &game, const Players &players)
{
    dts::unused(players);

    static std::set<std::string> ignoredMaps;

    int mapIndex = blitzmap::toIndex(game.mapName());
    int winner = game.winnerIndex();

    if (_gameMode == gamemodes::Blitz && mapIndex < 0)
    {
        // Not interested in some maps.
        if (ignoredMaps.find(game.mapName()) == ignoredMaps.end())
        {
            Log::info() << "Ignoring map '" << game.mapName() << "' while making map stats.";
            ignoredMaps.insert(game.mapName());
        }
        return;
    }

    std::string mapName = (_gameMode == gamemodes::Blitz) ? blitzmap::__names[mapIndex] : game.mapName();

    // Normalize map name for RA2.
    if (_gameMode == gamemodes::RedAlert2 && mapName.size() > 2)
    {
        if (mapName[0] >= '0' && mapName[0] <= '9')
        {
            mapName.erase(mapName.begin());
        }
        while (mapName[0] == ' ')
        {
            mapName.erase(mapName.begin());
        }
        std::regex reParenthesis("\\(.*?\\)");
        mapName = std::regex_replace(mapName, reParenthesis, "");
        std::regex reDoubleSpace("\\s{2,}");
        mapName = std::regex_replace(mapName, reDoubleSpace, " ");

        while (mapName[mapName.size() - 1] == ' ')
        {
            mapName.erase(std::prev(mapName.end()));
        }
    }

    std::chrono::year_month_day date{game.date().year(), game.date().month(), std::chrono::day{1}};

    _gameCountsPerMonthAndPlayer[date][mapName].count++;
    _gameCountsPerMonthAndPlayer[date][mapName].differentPlayers.insert(game.userId(0));
    _gameCountsPerMonthAndPlayer[date][mapName].differentPlayers.insert(game.userId(1));

    // Ignore games with duration 0. These are probably manually added game, e.g. tournament games.
    if (game.duration() > 0)
    {
        _averageDuration[mapName].first += game.duration();
        _averageDuration[mapName].second++;
    }

    // Not interested in draws.
    if (game.isDraw())
    {
        return;
    }

    // Process potential upset.
    double diff = game.differenceForGreatestDefeat();
    if (diff > 300.0)
    {
        // Loser needs to have deviation < 120 or must have been active before.
        if (!game.isBot() && (game.deviation(winner ^ 1) < 120 || players[game.userId(winner ^ 1)].wasActive()))
        {
            Upset upset{ game.date(), game.userId(winner), game.userId(winner ^ 1), mapName, game.faction(winner), game.faction(winner ^ 1), diff };
            _upsetsMonthly[date].insert(upset);
            if (_upsetsMonthly[date].size() > 20)
            {
                _upsetsMonthly[date].erase(std::prev(_upsetsMonthly[date].end()));
            }

            auto yearBoundary = floor<std::chrono::days>(std::chrono::system_clock::now()) - std::chrono::days(365);
            if (date >= yearBoundary)
            {
                _upsetsLast12Month.insert(upset);
            }
            if (_upsetsLast12Month.size() > 50)
            {
                _upsetsLast12Month.erase(std::prev(_upsetsLast12Month.end()));
            }

            auto monthBoundary = floor<std::chrono::days>(std::chrono::system_clock::now()) - std::chrono::days(31);
            if (date >= monthBoundary)
            {
                _upsetsLast30Days.insert(upset);
            }
            if (_upsetsLast30Days.size() > 50)
            {
                _upsetsLast30Days.erase(std::prev(_upsetsLast30Days.end()));
            }

            _upsetsAllTime.insert(upset);
            if (_upsetsAllTime.size() > 100)
            {
                _upsetsAllTime.erase(std::prev(_upsetsAllTime.end()));
            }
        }
    }

    // Process longest games.
    if (!game.isBot() && !game.isDraw() && game.duration() > 600 && game.fps() > 0)
    {
        // First, we normalize the duration.
        uint32_t duration = game.duration() * game.fps() / 59;
        Upset upset{ game.date(), game.userId(winner), game.userId(winner ^ 1), mapName, game.faction(winner), game.faction(winner ^ 1), diff, duration };
        _longestGames.insert(upset);
        if (_longestGames.size() > 25)
        {
            _longestGames.erase(std::prev(_longestGames.end()));
        }
    }

    // Now do the real map stats. Only interested in different faction games.
    if (game.faction(0) == game.faction(1))
    {
        return;
    }

    _gameCount++;

    factions::Setup factionSetup = game.setup();

    // Flip setups if neccessary.
    if (factionSetup == factions::SvA)
    {
        factionSetup = factions::AvS;
    }
    else if (factionSetup == factions::YvA)
    {
        factionSetup = factions::AvY;
    }
    else if (factionSetup == factions::SvY)
    {
        factionSetup = factions::YvS;
    }

    if (factionSetup == factions::AvS)
    {
        uint32_t alliedPlayerIndex = game.faction(0) == factions::Allied ? 0 : 1;
        uint32_t sovietPlayerIndex = game.faction(1) == factions::Soviet ? 0 : 1;

        Rating alliedRating(game.rating(alliedPlayerIndex), game.deviation(alliedPlayerIndex), glicko::initialVolatility);
        Rating sovietRating(game.rating(sovietPlayerIndex), game.deviation(sovietPlayerIndex), glicko::initialVolatility);

        double expectedWinRate = alliedRating.e_star(sovietRating.toArray(), 0.0);

        // 1300 is a good compromise for including games that actually matter.
        if ((alliedRating.elo() - alliedRating.deviation()) < 1300.0
            || (alliedRating.elo() - alliedRating.deviation()) < 1300.0
            || alliedRating.eloDeviation() > 100
            || sovietRating.eloDeviation() > 100)
        {
            return;
        }

        _mapStats[factionSetup][mapName].addGame(expectedWinRate, (game.winnerIndex() == static_cast<int>(alliedPlayerIndex)));
    }
    else if (factionSetup == factions::AvY)
    {
        uint32_t alliedPlayerIndex = game.faction(0) == factions::Allied ? 0 : 1;
        uint32_t yuriPlayerIndex = game.faction(0) == factions::Yuri ? 0 : 1;

        Rating alliedRating(game.rating(alliedPlayerIndex), game.deviation(alliedPlayerIndex), glicko::initialVolatility);
        Rating yuriRating(game.rating(yuriPlayerIndex), game.deviation(yuriPlayerIndex), glicko::initialVolatility);

        double expectedWinRate = alliedRating.e_star(yuriRating.toArray(), 0.0);

        // 1300 is a good compromise for including games that actually matter.
        if ((alliedRating.elo() - alliedRating.deviation()) < 1300.0
            || (alliedRating.elo() - alliedRating.deviation()) < 1300.0
            || alliedRating.eloDeviation() > 100
            || yuriRating.eloDeviation() > 100)
        {
            return;
        }

        _mapStats[factionSetup][mapName].addGame(expectedWinRate, (game.winnerIndex() == static_cast<int>(alliedPlayerIndex)));
    }
    else if (factionSetup == factions::YvS)
    {
        uint32_t sovietPlayerIndex = game.faction(0) == factions::Soviet ? 0 : 1;
        uint32_t yuriPlayerIndex = game.faction(0) == factions::Yuri ? 0 : 1;

        Rating sovietRating(game.rating(sovietPlayerIndex), game.deviation(sovietPlayerIndex), glicko::initialVolatility);
        Rating yuriRating(game.rating(yuriPlayerIndex), game.deviation(yuriPlayerIndex), glicko::initialVolatility);

        double expectedWinRate = yuriRating.e_star(sovietRating.toArray(), 0.0);

        // 1300 is a good compromise for including games that actually matter.
        if ((yuriRating.elo() - yuriRating.deviation()) < 1300.0
            || (yuriRating.elo() - yuriRating.deviation()) < 1300.0
            || yuriRating.eloDeviation() > 100
            || sovietRating.eloDeviation() > 100)
        {
            return;
        }

        _mapStats[factionSetup][mapName].addGame(expectedWinRate, (game.winnerIndex() == static_cast<int>(yuriPlayerIndex)));
    }
}

/*!
 */
void MapStats::finalize(const std::filesystem::path &directory, const Players &players)
{
    Log::info() << "Finalizing map statistics.";

    using json = nlohmann::json;

    static std::vector<factions::Setup> factionSetups = { factions::AvS, factions::AvY, factions::YvS };

    for (factions::Setup factionSetup : factionSetups)
    {
        factions::Faction firstFaction = factions::firstFaction(factionSetup);
        factions::Faction secondFaction = factions::secondFaction(factionSetup);
        std::string loweredFirstFactionName = stringtools::toLower(factions::name(firstFaction));
        std::string loweredSecondFactionName = stringtools::toLower(factions::name(secondFaction));

        json data = json::object();

        data["description"] = "Map statistics, sorted by most " + loweredFirstFactionName + " favoured maps";
        data["columns"] = json::array({
                                       { { "index", 0 }, { "header", "#" } , { "name", "rank" } },
                                       { { "index", 1 }, { "header", "Map" } , { "name", "map" } },
                                       { { "index", 2 }, { "header", "Games" } , { "name", "game_count" }, { "info", "Number of games taken into account. Some games are sorted out, e.g. very low level games or games with players on high deviation." } },
                                       { { "index", 3 }, { "header", factions::name(firstFaction) + " win rate" } , { "name", "win_rate" }, { "info", "The win rate considers elo. A value of e.g. 55% means that an allied player with the exact same elo as a soviet player, is expected to win 55% of the games they play on this map." } },
                                       { { "index", 4 }, { "header", "&#x2300; Duration" } , { "name", "average_duration" }, { "info", "The average game time on this map with this specific setup." } },
                                       });

        std::set<std::pair<std::string, const Probabilities&>> result;

        std::map<std::string, Probabilities> &mapStats = _mapStats[factionSetup];

        for (auto it = mapStats.begin(); it != mapStats.end(); ++it)
        {
            it->second.finalize();
            result.insert({ it->first, it->second });
        }

        std::string factionPercent = factions::name(firstFaction) + "%";
        while (factionPercent.size() < 8)
        {
            factionPercent += " ";
        }

        json jMaps = json::array();
        int counter = 1;

        std::stringstream ss;
        ss << "+--------------------+--------+------+--------+\n";
        ss << "|Map                 |" << factionPercent << "|Games |Avg.Dur.|\n";
        ss << "+--------------------+--------+------+--------+\n";

        for (auto it = result.begin(); it != result.end(); ++it)
        {
            if (it->second.count() >= 50)
            {
                // Need at least 50 games to have some stats.
                json jMap = json::object();

                jMap["map"] = it->first;
                jMap["expected"] = it->second.expected();
                jMap["actual"] = it->second.actual();
                double winRateRaw = it->second.result() * 100;
                std::ostringstream oss1;
                oss1 << std::fixed << std::setprecision(2) << winRateRaw;
                jMap["win_rate"] = oss1.str();
                jMap["win_rate_rounded"] = static_cast<uint32_t>(it->second.result() * 100);
                jMap[loweredFirstFactionName + "_wins"] = it->second.wins();
                jMap[loweredFirstFactionName + "_losses"] = it->second.losses();
                jMap["game_count"] = it->second.losses() + it->second.wins();

                uint32_t averageDuration = _averageDuration[it->first].first / _averageDuration[it->first].second;
                std::string theSeconds = std::to_string(averageDuration % 60);
                if (theSeconds.size() == 1)
                {
                    theSeconds = "0" + theSeconds;
                }
                jMap["average_duration"] = std::to_string(averageDuration / 60) + ":" + theSeconds;

                jMap["rank"] = counter++;

                if (_gameMode == gamemodes::Blitz)
                {
                    if (it->second.result() > 0.55)
                    {
                        jMap["result"] = "Much better for " + loweredFirstFactionName + ".";
                    }
                    else if (it->second.result() > 0.525)
                    {
                        jMap["result"] = "Better for " + loweredFirstFactionName + ".";
                    }
                    else if (it->second.result() > 0.51)
                    {
                        jMap["result"] = "Slightly better for " + loweredFirstFactionName + ".";
                    }
                    else if (it->second.result() > 0.49)
                    {
                        jMap["result"] = "Even map for both factions.";
                    }
                    else if (it->second.result() > 0.475)
                    {
                        jMap["result"] = "Slightly better for " + loweredSecondFactionName + ".";
                    }
                    else if (it->second.result() > 0.45)
                    {
                        jMap["result"] = "Better for " + loweredSecondFactionName + ".";
                    }
                    else
                    {
                        jMap["result"] = "Much better for " + loweredSecondFactionName + ".";
                    }
                }

                std::string mapName = it->first;
                while (mapName.size() < 20)
                {
                    mapName.push_back(' ');
                }


                double valueRaw = it->second.result() * 100;
                std::ostringstream oss2;
                oss2 << std::fixed << std::setprecision(2) << valueRaw;
                std::string value = oss2.str();
                while (value.size() < 8)
                {
                    value = " " + value;
                }

                std::string games = std::to_string(it->second.wins() + it->second.losses());
                while (games.size() < 6)
                {
                    games = " " + games;
                }

                std::string seconds = std::to_string(averageDuration % 60);
                if (seconds.size() == 1)
                {
                    seconds = "0" + seconds;
                }
                std::string duration = std::to_string(averageDuration / 60) + ":" + seconds;
                while (duration.size() < 8)
                {
                    duration = " " + duration;
                }
                ss << "|" << mapName << "|" << value << "|" << games << "|" << duration << "|\n";

                jMaps.push_back(jMap);
            }
        }
        ss << "+--------------------+--------+------+--------+\n";

        data["data"] = jMaps;

        if (jMaps.empty())
        {
            // No need to write something if there is no data.
            continue;
        }

        std::string mapstatsFile =  gamemodes::shortName(_gameMode) + "_mapstats_" + stringtools::toLower(factions::toString(factionSetup));

        std::ofstream stream(directory / (mapstatsFile + ".json"));
        stream << std::setw(4) << data << std::endl;
        stream.close();

        std::ofstream streamTxt(directory / (mapstatsFile + ".txt"));
        streamTxt << ss.str();
        streamTxt.close();
    }

    Log::info() << "Map statistics created.";
}

/*!
 */
void MapStats::exportMapsPlayed(const std::filesystem::path &directory)
{
    using json = nlohmann::json;

    // Yearly map.
    std::map<int, std::map<std::string, MapPlayed>> yearlyMaps;

    for (auto it = _gameCountsPerMonthAndPlayer.begin(); it != _gameCountsPerMonthAndPlayer.end(); ++it)
    {
        int year = static_cast<int>(it->first.year());
        int month = static_cast<unsigned>(it->first.month());

        std::set<MapPlayed> playedMaps;
        const std::map<std::string, MapPlayed> &mapsPlayed = it->second;

        for (auto it2 = mapsPlayed.begin(); it2 != mapsPlayed.end(); ++it2)
        {
            std::string mapName = it2->first;
            MapPlayed mapPlayed = it2->second;
            mapPlayed.mapName = mapName;
            playedMaps.insert(mapPlayed);
            yearlyMaps[year][mapName].count += mapPlayed.count;
            yearlyMaps[year][mapName].mapName = mapName;
            yearlyMaps[year][mapName].differentPlayers.insert(mapPlayed.differentPlayers.begin(), mapPlayed.differentPlayers.end());
        }

        std::stringstream ssMapsPlayed;
        ssMapsPlayed << "+--------------------+------+--------+\n";
        ssMapsPlayed << "|Map                 |Games |Players |\n";
        ssMapsPlayed << "+--------------------+------+--------+\n";

        json maps_played = json::array();
        int rank = 1;
        for (auto itSet = playedMaps.begin(); itSet != playedMaps.end(); ++itSet)
        {
            json map_played = json::object();
            map_played["rank"] = rank++;
            map_played["name"] = itSet->mapName;
            map_played["count"] = itSet->count;
            map_played["different_players"] = itSet->differentPlayers.size();
            maps_played.push_back(map_played);

            std::string mapName = itSet->mapName;
            while (mapName.size() < 20)
            {
                mapName.push_back(' ');
            }

            std::string games = std::to_string(itSet->count);
            while (games.size() < 6)
            {
                games = " " + games;
            }

            std::string diffPlayers = std::to_string(itSet->differentPlayers.size());
            while (diffPlayers.size() < 8)
            {
                diffPlayers = " " + diffPlayers;
            }
            ssMapsPlayed << "|" << mapName << "|" << games << "|" << diffPlayers << "|\n";

        }

        ssMapsPlayed << "+--------------------+------+--------+\n";

        std::stringstream ss;
        ss << gamemodes::shortName(_gameMode) << "_maps_played_" << year << "-" << (month < 10 ? "0" : "") << month << ".json";
        std::ofstream stream(directory / ss.str());
        stream << std::setw(4) << maps_played << std::endl;
        stream.close();

        std::string filenameTxt = std::regex_replace(ss.str(), std::regex("\\json"), "txt");
        std::ofstream streamTxt(directory / filenameTxt);
        streamTxt << ssMapsPlayed.str();
        streamTxt.close();
    }

    std::set<MapPlayed> playedMaps;
    int year = 2024;
    for (auto it = yearlyMaps[year].begin(); it != yearlyMaps[year].end(); ++it)
    {
        playedMaps.insert(it->second);
    }
    std::stringstream ssMapsPlayed;
    ssMapsPlayed << "+--------------------+------+--------+\n";
    ssMapsPlayed << "|Map                 |Games |Players |\n";
    ssMapsPlayed << "+--------------------+------+--------+\n";

    json maps_played = json::array();
    int rank = 1;
    for (auto itSet = playedMaps.begin(); itSet != playedMaps.end(); ++itSet)
    {
        json map_played = json::object();
        map_played["rank"] = rank++;
        map_played["name"] = itSet->mapName;
        map_played["count"] = itSet->count;
        map_played["different_players"] = itSet->differentPlayers.size();
        maps_played.push_back(map_played);

        std::string mapName = itSet->mapName;
        while (mapName.size() < 20)
        {
            mapName.push_back(' ');
        }

        std::string games = std::to_string(itSet->count);
        while (games.size() < 6)
        {
            games = " " + games;
        }

        std::string diffPlayers = std::to_string(itSet->differentPlayers.size());
        while (diffPlayers.size() < 8)
        {
            diffPlayers = " " + diffPlayers;
        }
        ssMapsPlayed << "|" << mapName << "|" << games << "|" << diffPlayers << "|\n";

    }

    ssMapsPlayed << "+--------------------+------+--------+\n";

    std::stringstream ss;
    ss << gamemodes::shortName(_gameMode) << "_maps_played_" << year << ".json";
    std::ofstream stream(directory / ss.str());
    stream << std::setw(4) << maps_played << std::endl;
    stream.close();

    std::string filenameTxt = std::regex_replace(ss.str(), std::regex("\\json"), "txt");
    std::ofstream streamTxt(directory / filenameTxt);
    streamTxt << ssMapsPlayed.str();
    streamTxt.close();
}

/*!
 */
void MapStats::exportLongestGames(const std::filesystem::path &directory, const Players &players)
{
    using json = nlohmann::json;

    // Process longest games.
    json jLongestGames = json::array();
    int rank = 1;
    for (auto it = _longestGames.begin(); it != _longestGames.end(); ++it)
    {
        json jLongestGame = json::object();
        jLongestGame["rank"] = rank++;
        jLongestGame["date"] = stringtools::fromDate(it->date);
        jLongestGame["winner"] = players[it->winner].alias();
        jLongestGame["loser"] = players[it->loser].alias();
        jLongestGame["winner_faction"] = factions::shortName(it->factionWinner);
        jLongestGame["loser_faction"] = factions::shortName(it->factionLoser);
        jLongestGame["map"] = it->map;
        jLongestGame["duration_seconds"] = it->duration;
        jLongestGames.push_back(jLongestGame);
    }

    std::ofstream streamLongest(directory / (gamemodes::shortName(_gameMode) + "_longest_games.json"));
    streamLongest << std::setw(4) << jLongestGames << std::endl;
    streamLongest.close();
}

/*!
 */
void MapStats::exportUpsets(const std::filesystem::path &directory, const Players &players)
{
    // Now process upsets.
    for (auto it = _upsetsMonthly.begin(); it != _upsetsMonthly.end(); ++it)
    {
        int year = static_cast<int>(it->first.year());
        int month = static_cast<unsigned>(it->first.month());

        std::stringstream ss;
        ss << gamemodes::shortName(_gameMode) << "_upsets_" << year << "-" << (month < 10 ? "0" : "") << month << ".json";

        exportUpsets(directory, it->second, ss.str(), "", players);
    }

    exportUpsets(directory, _upsetsLast12Month, gamemodes::shortName(_gameMode) + "_upsets_last12month.json", "Upsets within the last 12 month", players);
    exportUpsets(directory, _upsetsLast30Days, gamemodes::shortName(_gameMode) + "_upsets_last30days.json", "Upsets within the last 30 days", players);
    exportUpsets(directory, _upsetsAllTime, gamemodes::shortName(_gameMode) + "_upsets_alltime.json", "Biggest upsets of all time", players);
}

/*!
 */
void MapStats::exportUpsets(
    const std::filesystem::path &directory,
    const std::multiset<Upset> &upsets,
    const std::string &filename,
    const std::string &description,
    const Players &players) const
{
    using json = nlohmann::json;

    json data = json::object();

    data["description"] = description;
    data["columns"] = json::array({
        { { "index", 0 }, { "header", "#" } , { "name", "rank" } },
        { { "index", 1 }, { "header", "Date" } , { "name", "date" } },
        { { "index", 2 }, { "header", "" } , { "name", "faction_winner" }, { "info", "Winners faction." } },
        { { "index", 3 }, { "header", "Winner" } , { "name", "winner" } },
        { { "index", 4 }, { "header", "" } , { "name", "faction_loser" }, { "info", "Losers faction." } },
        { { "index", 5 }, { "header", "Loser" } , { "name", "loser" } },
        { { "index", 6 }, { "header", "Map" } , { "name", "map" } },
        { { "index", 7 }, { "header", "Diff" } , { "name", "rating_difference" }, { "info", "Difference in elo rating."} }
    });

    json jUpsets = json::array();
    int rank = 1;

    for (auto it = upsets.begin(); it != upsets.end(); ++it)
    {
        json jUpset = json::object();
        jUpset["rank"] = rank++;
        jUpset["date"] = stringtools::fromDate(it->date);
        jUpset["winner"] = players[it->winner].alias();
        jUpset["loser"] = players[it->loser].alias();
        jUpset["faction_winner"] = factions::shortName(it->factionWinner);
        jUpset["faction_loser"] = factions::shortName(it->factionLoser);
        jUpset["map"] = it->map;
        jUpset["rating_difference"] = std::to_string(static_cast<int>(it->eloDifference));
        jUpsets.push_back(jUpset);
    }

    data["data"] = jUpsets;

    std::ofstream stream(directory / filename);
    stream << std::setw(4) << data << std::endl;
    stream.close();
}
