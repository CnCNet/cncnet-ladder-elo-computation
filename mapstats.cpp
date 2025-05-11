
// C++ includes:
#include <algorithm>
#include <fstream>
#include <regex>

// Local includes:
#include "cplusplus.h"
//#include "global.h"
#include "knownplayers.h"
#include "logging.h"
//#include "map.h"
#include "mapstats.h"

// TODO:
// - Most games a day/month/year

/*!
 */
MapStats::MapStats() :
    _longestGames([](const Upset &a, const Upset &b) { return a.duration > b.duration; })
{
}

/*!
 */
void MapStats::processGame(const Game &game, const Players &players)
{
    dts::unused(players);

    static std::set<std::string> ignoredMaps;

    int mapIndex = map::toIndex(game.mapName());
    int winner = game.winnerIndex();

    if (g_gameMode == gamemodes::Blitz && mapIndex < 0)
    {
        // Not interested in some maps.
        if (ignoredMaps.find(game.mapName().toStdString()) == ignoredMaps.end())
        {
            Log::info() << "Ignoring map '" << game.mapName() << "' while making map stats.";
            ignoredMaps.insert(game.mapName().toStdString());
        }
        return;
    }

    std::string mapName = (g_gameMode == gamemodes::Blitz) ? map::__names[mapIndex].toStdString() : game.mapName().toStdString();

    // Normalize map name for RA2.
    if (g_gameMode == gamemodes::RedAlert2 && mapName.size() > 2)
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

    QDate date(game.date().year(), game.date().month(), 1);
    _gameCountsPerMonthAndPlayer[date][mapName].count++;
    _gameCountsPerMonthAndPlayer[date][mapName].differentPlayers.insert(game.player(0));
    _gameCountsPerMonthAndPlayer[date][mapName].differentPlayers.insert(game.player(1));
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
        if (!game.isBot() && (game.deviation(winner ^ 1) < 120 || players[game.player(winner ^ 1)].wasActive()))
        {
            Upset upset{ game.date(), game.player(winner), game.player(winner ^ 1), mapName, game.faction(winner), game.faction(winner ^ 1), diff };
            _upsetsMonthly[date].insert(upset);
            if (_upsetsMonthly[date].size() > 20)
            {
                _upsetsMonthly[date].erase(std::prev(_upsetsMonthly[date].end()));
            }

            QDate yearBoundary = QDate::currentDate().addDays(-365);
            if (date >= yearBoundary)
            {
                _upsetsLast12Month.insert(upset);
            }
            if (_upsetsLast12Month.size() > 50)
            {
                _upsetsLast12Month.erase(std::prev(_upsetsLast12Month.end()));
            }

            QDate monthBoundary = QDate::currentDate().addDays(-31);
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
        Upset upset{ game.date(), game.player(winner), game.player(winner ^ 1), mapName, game.faction(winner), game.faction(winner ^ 1), diff, duration };
        _longestGames.insert(upset);
        if (_longestGames.size() > 25)
        {
            _longestGames.erase(std::prev(_longestGames.end()));
        }
    }

    // Now do the real map stats. Only interested in different faction games.
    if (game.faction1() == game.faction2())
    {
        return;
    }

    gameCount++;

    gamesetup::Setup factionSetup = game.setup();

    // Flip setups if neccessary.
    if (factionSetup == gamesetup::SvA)
    {
        factionSetup = gamesetup::AvS;
    }
    else if (factionSetup == gamesetup::YvA)
    {
        factionSetup = gamesetup::AvY;
    }
    else if (factionSetup == gamesetup::SvY)
    {
        factionSetup = gamesetup::YvS;
    }

    if (factionSetup == gamesetup::AvS)
    {
        uint32_t alliedPlayerIndex = game.faction1() == factions::Allied ? 0 : 1;
        uint32_t sovietPlayerIndex = game.faction1() == factions::Soviet ? 0 : 1;

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
    else if (factionSetup == gamesetup::AvY)
    {
        uint32_t alliedPlayerIndex = game.faction1() == factions::Allied ? 0 : 1;
        uint32_t yuriPlayerIndex = game.faction1() == factions::Yuri ? 0 : 1;

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
    else if (factionSetup == gamesetup::YvS)
    {
        uint32_t sovietPlayerIndex = game.faction1() == factions::Soviet ? 0 : 1;
        uint32_t yuriPlayerIndex = game.faction1() == factions::Yuri ? 0 : 1;

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
void MapStats::finalize(const Players &players)
{
    Log::info() << "Finalizing map statistics.";

    using json = nlohmann::json;

    static std::vector<gamesetup::Setup> factionSetups = { gamesetup::AvS, gamesetup::AvY, gamesetup::YvS };

    for (gamesetup::Setup factionSetup : factionSetups)
    {
        factions::Faction firstFaction = gamesetup::firstFaction(factionSetup);
        factions::Faction secondFaction = gamesetup::secondFaction(factionSetup);
        std::string loweredFirstFactionName = factions::name(firstFaction).toLower().toStdString();
        std::string loweredSecondFactionName = factions::name(secondFaction).toLower().toStdString();

        json data = json::object();

        data["description"] = "Map statistics, sorted by most " + loweredFirstFactionName + " favoured maps";
        data["columns"] = json::array({
                                       { { "index", 0 }, { "header", "#" } , { "name", "rank" } },
                                       { { "index", 1 }, { "header", "Map" } , { "name", "map" } },
                                       { { "index", 2 }, { "header", "Games" } , { "name", "game_count" }, { "info", "Number of games taken into account. Some games are sorted out, e.g. very low level games or games with players on high deviation." } },
                                       { { "index", 3 }, { "header", factions::name(firstFaction).toStdString() + " win rate" } , { "name", "win_rate" }, { "info", "The win rate considers elo. A value of e.g. 55% means that an allied player with the exact same elo as a soviet player, is expected to win 55% of the games they play on this map." } },
                                       { { "index", 4 }, { "header", "&#x2300; Duration" } , { "name", "average_duration" }, { "info", "The average game time on this map with this specific setup." } },
                                       });

        std::set<std::pair<std::string, const Probabilities&>> result;

        QMap<std::string, Probabilities> &mapStats = _mapStats[factionSetup];

        for (auto it = mapStats.begin(); it != mapStats.end(); ++it)
        {
            it.value().finalize();
            result.insert({ it.key(), it.value() });
        }

        std::string factionPercent = factions::name(firstFaction).toStdString() + "%";
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
                jMap["win_rate"] = (QString::number(it->second.result() * 100, 'f', 2) + "%").toStdString();
                jMap["win_rate_rounded"] = static_cast<uint32_t>(it->second.result() * 100);
                jMap[loweredFirstFactionName + "_wins"] = it->second.wins();
                jMap[loweredFirstFactionName + "_losses"] = it->second.losses();
                jMap["game_count"] = it->second.losses() + it->second.wins();

                uint32_t averageDuration = _averageDuration[it->first].first / _averageDuration[it->first].second;
                QString theSeconds = QString::number(averageDuration % 60);
                if (theSeconds.size() == 1)
                {
                    theSeconds = "0" + theSeconds;
                }
                jMap["average_duration"] = QString("%1:%2").arg(averageDuration / 60).arg(theSeconds).toStdString();

                jMap["rank"] = counter++;

                if (g_gameMode == gamemodes::Blitz)
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

                QString value = QString::number(it->second.result() * 100, 'f', 2);
                while (value.size() < 8)
                {
                    value = " " + value;
                }

                QString games = QString::number(it->second.wins() + it->second.losses());
                while (games.size() < 6)
                {
                    games = " " + games;
                }

                QString seconds = QString::number(averageDuration % 60);
                if (seconds.size() == 1)
                {
                    seconds = "0" + seconds;
                }
                QString duration = QString("%1:%2").arg(averageDuration / 60).arg(seconds);
                while (duration.size() < 8)
                {
                    duration = " " + duration;
                }
                ss << "|" << mapName << "|" << value.toStdString() << "|" << games.toStdString() << "|" << duration.toStdString() << "|\n";

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

        std::string mapstatsFile = QString("%1_mapstats_%2").arg(gamemodes::shortName(g_gameMode), QString::fromStdString(gamesetup::toString(factionSetup)).toLower()).toStdString();

        std::ofstream stream(pathFromFilename(mapstatsFile + ".json"));
        stream << std::setw(4) << data << std::endl;
        stream.close();

        std::ofstream streamTxt(pathFromFilename(mapstatsFile + ".txt"));
        streamTxt << ss.str();
        streamTxt.close();
    }

    Log::info() << "Map statistics created.";
}

void MapStats::exportMapsPlayed()
{
    using json = nlohmann::json;

    // Yearly map.
    QMap<int, QMap<std::string, MapPlayed>> yearlyMaps;

    for (auto it = _gameCountsPerMonthAndPlayer.begin(); it != _gameCountsPerMonthAndPlayer.end(); ++it)
    {
        int year = it.key().year();
        int month = it.key().month();

        std::set<MapPlayed> playedMaps;
        const QMap<std::string, MapPlayed> &mapsPlayed = it.value();

        for (auto it2 = mapsPlayed.begin(); it2 != mapsPlayed.end(); ++it2)
        {
            std::string mapName = it2.key();
            MapPlayed mapPlayed = it2.value();
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

            QString games = QString::number(itSet->count);
            while (games.size() < 6)
            {
                games = " " + games;
            }

            QString diffPlayers = QString::number(itSet->differentPlayers.size());
            while (diffPlayers.size() < 8)
            {
                diffPlayers = " " + diffPlayers;
            }
            ssMapsPlayed << "|" << mapName << "|" << games.toStdString() << "|" << diffPlayers.toStdString() << "|\n";

        }

        ssMapsPlayed << "+--------------------+------+--------+\n";

        std::stringstream ss;
        ss << "maps_played_" << year << "-" << (month < 10 ? "0" : "") << month << ".json";
        std::ofstream stream(pathFromFilename(ss.str()));
        stream << std::setw(4) << maps_played << std::endl;
        stream.close();

        std::string filenameTxt = std::regex_replace(ss.str(), std::regex("\\json"), "txt");
        std::ofstream streamTxt(pathFromFilename(filenameTxt));
        streamTxt << ssMapsPlayed.str();
        streamTxt.close();
    }

    std::set<MapPlayed> playedMaps;
    int year = 2024;
    for (auto it = yearlyMaps[year].begin(); it != yearlyMaps[year].end(); ++it)
    {
        playedMaps.insert(it.value());
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

        QString games = QString::number(itSet->count);
        while (games.size() < 6)
        {
            games = " " + games;
        }

        QString diffPlayers = QString::number(itSet->differentPlayers.size());
        while (diffPlayers.size() < 8)
        {
            diffPlayers = " " + diffPlayers;
        }
        ssMapsPlayed << "|" << mapName << "|" << games.toStdString() << "|" << diffPlayers.toStdString() << "|\n";

    }

    ssMapsPlayed << "+--------------------+------+--------+\n";

    std::stringstream ss;
    ss << "maps_played_" << year << ".json";
    std::ofstream stream(pathFromFilename(ss.str()));
    stream << std::setw(4) << maps_played << std::endl;
    stream.close();

    std::string filenameTxt = std::regex_replace(ss.str(), std::regex("\\json"), "txt");
    std::ofstream streamTxt(pathFromFilename(filenameTxt));
    streamTxt << ssMapsPlayed.str();
    streamTxt.close();
}

void MapStats::exportLongestGames(const Players &players)
{
    using json = nlohmann::json;

    // Process longest games.
    json jLongestGames = json::array();
    int rank = 1;
    for (auto it = _longestGames.begin(); it != _longestGames.end(); ++it)
    {
        json jLongestGame = json::object();
        jLongestGame["rank"] = rank++;
        jLongestGame["date"] = it->date.toString("yyyy-MM-dd").toStdString();
        jLongestGame["winner"] = players[it->winner].alias(true).toStdString();
        jLongestGame["loser"] = players[it->loser].alias(true).toStdString();
        jLongestGame["winner_faction"] = factions::shortName(it->factionWinner).toStdString();
        jLongestGame["loser_faction"] = factions::shortName(it->factionLoser).toStdString();
        jLongestGame["map"] = it->map;
        jLongestGame["duration_seconds"] = it->duration;
        jLongestGames.push_back(jLongestGame);
    }

    std::ofstream streamLongest(pathFromFilename("longest_games.json"));
    streamLongest << std::setw(4) << jLongestGames << std::endl;
    streamLongest.close();
}

void MapStats::exportUpsets(const Players &players)
{
    // Now process upsets.
    for (auto it = _upsetsMonthly.begin(); it != _upsetsMonthly.end(); ++it)
    {
        int year = it.key().year();
        int month = it.key().month();

        std::stringstream ss;
        ss << "upsets_" << year << "-" << (month < 10 ? "0" : "") << month << ".json";

        exportUpsets(it.value(), ss.str(), "", players);
    }

    exportUpsets(_upsetsLast12Month, QString("%1_upsets_last12month.json").arg(gamemodes::shortName(g_gameMode)).toStdString(), "Upsets within the last 12 month", players);
    exportUpsets(_upsetsLast30Days, QString("%1_upsets_last30days.json").arg(gamemodes::shortName(g_gameMode)).toStdString(), "Upsets within the last 30 days", players);
    exportUpsets(_upsetsAllTime, QString("%1_upsets_alltime.json").arg(gamemodes::shortName(g_gameMode)).toStdString(), "Biggest upsets of all time", players);
}

/*!
 */
void MapStats::exportUpsets(
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
        jUpset["date"] = it->date.toString("yyyy-MM-dd").toStdString();
        jUpset["winner"] = players[it->winner].alias(true).toStdString();
        jUpset["loser"] = players[it->loser].alias(true).toStdString();
        jUpset["faction_winner"] = factions::shortName(it->factionWinner).toStdString();
        jUpset["faction_loser"] = factions::shortName(it->factionLoser).toStdString();
        jUpset["map"] = it->map;
        jUpset["rating_difference"] = QString::number(it->eloDifference, 'f', 0).toStdString();
        jUpsets.push_back(jUpset);
    }

    data["data"] = jUpsets;

    std::ofstream stream(pathFromFilename(filename));
    stream << std::setw(4) << data << std::endl;
    stream.close();
}


