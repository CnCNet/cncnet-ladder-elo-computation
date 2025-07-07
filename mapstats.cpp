
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

    std::chrono::year_month_day keyDate{game.date().year(), game.date().month(), std::chrono::day{1}};
    std::chrono::sys_days gameDate = game.sysDate();

    _gameCountsPerMonthAndPlayer[keyDate][mapName].count++;
    _gameCountsPerMonthAndPlayer[keyDate][mapName].differentPlayers.insert(game.userId(0));
    _gameCountsPerMonthAndPlayer[keyDate][mapName].differentPlayers.insert(game.userId(1));

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

    auto losers = [&](const Game& g) -> std::vector<uint32_t> {
        return g.collectFromParticipants<uint32_t>([](const Game::Participant& p) {
                return std::pair<uint32_t, bool>{p.userId, !p.hasWon};
            });};

    auto winners = [&](const Game& g) -> std::vector<uint32_t> {
        return g.collectFromParticipants<uint32_t>([](const Game::Participant& p) {
            return std::pair<uint32_t, bool>{p.userId, p.hasWon};
        });};

    auto winnersFactions =  [&](const Game& g) -> std::vector<factions::Faction> {
        return g.collectFromParticipants<factions::Faction>([](const Game::Participant& p) {
            return std::pair<factions::Faction, bool>{p.faction, p.hasWon};
        });};

    auto losersFactions =  [&](const Game& g) -> std::vector<factions::Faction> {
        return g.collectFromParticipants<factions::Faction>([](const Game::Participant& p) {
            return std::pair<factions::Faction, bool>{p.faction, !p.hasWon};
        });};

    auto winnersELOs = [&](const Game& g) -> std::vector<int> {
        return g.collectFromParticipants<int>([](const Game::Participant& p) {
            return std::pair<int, bool>{p.elo, p.hasWon};
        });};

    auto losersELOs = [&](const Game& g) -> std::vector<int> {
        return g.collectFromParticipants<int>([](const Game::Participant& p) {
            return std::pair<int, bool>{p.elo, !p.hasWon};
        });};

    // Process potential upset.
    double diff = game.differenceForGreatestDefeat();
    if (diff > 300.0)
    {
        // Loser needs to have deviation < 120 or must have been active before.
        if (!game.isBot() &&
            game.allParticipants([&](const Game::Participant& p) {
                return p.hasWon || (!p.hasWon && (p.deviation < 120.0 || players[p.userId].wasActive())); }))
        {

            Log::warning() << game << " ==> " << diff;

            std::vector<uint32_t> winnerIds = winners(game);
            std::vector<uint32_t> loserIds = losers(game);
            std::vector<factions::Faction> winnerFactions = winnersFactions(game);
            std::vector<factions::Faction> loserFactions = losersFactions(game);
            std::vector<int> winnerELOs = winnersELOs(game);
            std::vector<int> loserELOs = losersELOs(game);

            Upset upset{ game.date(), winnerIds, loserIds, mapName, winnerFactions, loserFactions, winnerELOs, loserELOs, diff };
            _upsetsMonthly[keyDate].insert(upset);
            if (_upsetsMonthly[keyDate].size() > 20)
            {
                _upsetsMonthly[keyDate].erase(std::prev(_upsetsMonthly[keyDate].end()));
            }

            std::chrono::time_point<std::chrono::system_clock, std::chrono::days> today
                = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());

            std::chrono::time_point<std::chrono::system_clock, std::chrono::days> yearBoundary
                = today - std::chrono::days(365);

            if (gameDate >= yearBoundary)
            {
                _upsetsLast12Month.insert(upset);
            }
            if (_upsetsLast12Month.size() > 50)
            {
                _upsetsLast12Month.erase(std::prev(_upsetsLast12Month.end()));
            }

            std::chrono::time_point<std::chrono::system_clock, std::chrono::days> monthBoundary
                = today - std::chrono::days(31);


            if (gameDate >= monthBoundary)
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

    // Process team stats.
    if (_gameMode == gamemodes::Blitz2v2)
    {
        auto gameWinners = [&](const Game& g) -> std::vector<Game::Participant> {
            return g.collectFromParticipants<Game::Participant>([](const Game::Participant& p) {
                return std::pair<Game::Participant, bool>{p, p.hasWon};
            });};

        auto gameLosers = [&](const Game& g) -> std::vector<Game::Participant> {
            return g.collectFromParticipants<Game::Participant>([](const Game::Participant& p) {
                return std::pair<Game::Participant, bool>{p, !p.hasWon};
            });};

        std::vector<Game::Participant> winners = gameWinners(game);
        std::vector<Game::Participant> losers = gameLosers(game);
        assert(winners.size() == 2 && losers.size() == 2);

        std::set<uint64_t> winnerIds = { winners[0].userId, winners[1].userId };
        std::set<uint64_t> loserIds = { losers[0].userId, losers[1].userId };
        uint64_t winnerTeamId = ((*winnerIds.begin()) << 32) | *winnerIds.rbegin();
        uint64_t loserTeamId = ((*loserIds.begin()) << 32) | *loserIds.rbegin();

        Probabilities &probsWinners = _teamStats[winnerTeamId];
        Probabilities &probsLosers = _teamStats[loserTeamId];

        double winnerFirstElo = (winners[0].userId < winners[1].userId) ? winners[1].elo : winners[0].elo;
        double winnerSecondElo = (winners[1].userId > winners[0].userId) ? winners[0].elo : winners[1].elo;
        _lastTeamELOs[winnerTeamId].push_back({winnerFirstElo, winnerSecondElo});

        double loserFirstElo = (losers[0].userId < losers[1].userId) ? losers[1].elo : losers[0].elo;
        double loserSecondElo = (losers[1].userId > losers[0].userId) ? losers[0].elo : losers[1].elo;
        _lastTeamELOs[loserTeamId].push_back({loserFirstElo, loserSecondElo});

        Rating winnerRating(winners[0].elo + winners[1].elo, winners[0].deviation + winners[1].deviation, glicko::initialVolatility);
        Rating losersRating(losers[0].elo + losers[1].elo, losers[0].deviation + losers[1].deviation, glicko::initialVolatility);

        double expectedWinRate = winnerRating.e_star(losersRating.toArray(), 0.0);

        probsWinners.addGame(expectedWinRate, game.sysDate(), 1);
        probsLosers.addGame(1.0 - expectedWinRate, game.sysDate(), 0);
    }

    // Process longest games.
    if (!game.isBot() && !game.isDraw() && game.duration() > 600 && game.fps() > 0)
    {
        // First, we normalize the duration.
        uint32_t duration = game.duration() * game.fps() / 59;
        Upset upset{ game.date(), winners(game), losers(game), mapName, winnersFactions(game), losersFactions(game), winnersELOs(game), losersELOs(game), diff, duration };
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

    if (game.playerCount() > 2)
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
        uint32_t sovietPlayerIndex = game.faction(0) == factions::Soviet ? 0 : 1;

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

        _mapStats[factionSetup][mapName].addGame(expectedWinRate, game.sysDate(), (game.winnerIndex() == static_cast<int>(alliedPlayerIndex)));
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

        _mapStats[factionSetup][mapName].addGame(expectedWinRate, game.sysDate(), (game.winnerIndex() == static_cast<int>(alliedPlayerIndex)));
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

        _mapStats[factionSetup][mapName].addGame(expectedWinRate, game.sysDate(), (game.winnerIndex() == static_cast<int>(yuriPlayerIndex)));
    }

} // void MapStats::processGame(const Game &game, const Players &players)

/*!
 */
void MapStats::finalize(const std::filesystem::path &directory, const Players &players, std::chrono::sys_days date)
{
    Log::info() << "Finalizing map statistics.";

    using json = nlohmann::json;

    static std::vector<factions::Setup> factionSetups = { factions::AvS, factions::AvY, factions::YvS };

    Log::info() << "Creating team stats.";

    for (std::map<uint64_t, Probabilities>::iterator it = _teamStats.begin(); it != _teamStats.end(); ++it)
    {
        uint64_t key = it->first;
        Probabilities &value = it->second;
        value.finalize();
        ProbResult probToday = value.result(date);
        ProbResult probYesterday = value.result(date - std::chrono::days{3});
        uint32_t player1 = static_cast<uint32_t>(key & 0xFFFFFFFF);
        uint32_t player2 = static_cast<uint32_t>(key >> 32);

        auto eloDiff = [](const ProbResult &result) -> double {
            return -400.0 * std::log10((1.0 / result.normalized) - 1.0);
        };
        // Exclude teams where no player has ELO > 1300.
        if (probToday.games >= 20 && players[player1].isActive() && players[player2].isActive() && probToday.wins > 1 &&
            probToday.games != probToday.wins &&
            (players[player1].elo(factions::Combined) > 1300 || players[player2].elo(factions::Combined) > 1300))
        {
            Log::info() << players[player1].alias() << " + " << players[player2].alias() << " " << probToday.games << "/" << probToday.wins << " " << probToday.actual << "/" << probToday.expected;
            _teams.insert({key, probToday.games, probToday.wins, players[player1].elo(factions::Combined) + players[player2].elo(factions::Combined), eloDiff(probToday), probToday.lastGame });
        }

        if (probYesterday.games >= 20 && players[player1].isActive() && players[player2].isActive() && probYesterday.wins > 1 &&
            probYesterday.games != probYesterday.wins &&
            (players[player1].elo(factions::Combined) > 1300 || players[player2].elo(factions::Combined) > 1300))
        {
            Log::info() << players[player1].alias() << " + " << players[player2].alias() << " " << probYesterday.games << "/" << probYesterday.wins << " " << probYesterday.actual << "/" << probYesterday.expected;
            _yesterdaysTeams.insert({key, probYesterday.games, probYesterday.wins, players[player1].elo(factions::Combined) + players[player2].elo(factions::Combined), eloDiff(probYesterday), probYesterday.lastGame });
        }
    }

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
        if (it->winners.empty() || it->losers.empty())
        {
            Log::error() << "No winners or losers while exporting longest game.";
            continue;
        }

        json jLongestGame = json::object();
        jLongestGame["rank"] = rank++;
        jLongestGame["date"] = stringtools::fromDate(it->date);
        jLongestGame["winner"] = players[it->winners.front()].alias() +
                                 ((it->winners.size() > 1) ? "/" + players[it->winners[1]].alias() : "");
        jLongestGame["loser"] = players[it->losers.front()].alias() +
                                ((it->losers.size() > 1) ? "/" + players[it->losers[1]].alias() : "");

        jLongestGame["winner_faction"] = factions::shortName(it->winnerFaction());
        jLongestGame["loser_faction"] = factions::shortName(it->loserFaction());
        jLongestGame["map"] = it->map;
        jLongestGame["duration_seconds"] = it->duration;
        jLongestGames.push_back(jLongestGame);
    }

    std::ofstream streamLongest(directory / (gamemodes::shortName(_gameMode) + "_longest_games.json"));
    streamLongest << std::setw(4) << jLongestGames << std::endl;
    streamLongest.close();

    Log::verbose() << "Exported longest games.";
}

/*!
 */
void MapStats::exportBestTeams(const std::filesystem::path &directory, const Players &players)
{
    using json = nlohmann::json;

    json data = json::object();

    data["description"] = "Top 40 teams with the highest performance above predicted ELO";
    data["columns"] = json::array({
        { { "index", 0 }, { "header", "#" } , { "name", "rank" } },
        { { "index", 1 }, { "header", "∆ #" } , { "name", "delta_rank" } },
        { { "index", 2 }, { "header", "Names" } , { "name", "names" } },
        { { "index", 3 }, { "header", "Games" } , { "name", "games" } },
        { { "index", 4 }, { "header", "∆ Games" } , { "name", "delta_gamesplayed" } },
        { { "index", 5 }, { "header", "Last game" } , { "name", "last_game" } },
        { { "index", 6 }, { "header", "Team ELO" } , { "name", "elo_team" } },
        { { "index", 7 }, { "header", "Performance" } , { "name", "performance" } },
        { { "index", 8 }, { "header", "Difference" } , { "name", "diff" }, { "info", "Performance above team ELO."} },
        { { "index", 9 }, { "header", "∆" } , { "name", "delta_diff" }, { "info", "Performance above team ELO change since the day before."} }
    });

    int rank = 1;
    json jTeams = json::array();

    // Now process upsets.
    for (const Team &team : _teams)
    {
        json jTeam = json::object();
        jTeam["rank"] = rank++;
        jTeam["last_game"] = stringtools::fromDate(team.lastGame);

        uint32_t elo1 = static_cast<uint32_t>(std::round(_lastTeamELOs[team.teamId][team.games - 1].first));
        uint32_t elo2 = static_cast<uint32_t>(std::round(_lastTeamELOs[team.teamId][team.games - 1].second));

        std::string player1 = players[team.player1()].alias() + " (" + std::to_string(elo1)  + ")";
        std::string player2 = players[team.player2()].alias() + " (" + std::to_string(elo2)  + ")";
        jTeam["names"] = player1 + " / " + player2;
        jTeam["elo_team"] = std::to_string(elo1 + elo2);
        jTeam["games"] = std::to_string(team.games);
        jTeam["performance"] = std::to_string(elo1 + elo2 + static_cast<uint32_t>(std::round(team.eloDifference)));
        jTeam["diff"] = std::to_string(static_cast<uint32_t>(std::round(team.eloDifference)));

        int deltaRank = 0;
        int deltaGames = 0;
        int deltaDiff = 0;
        int currentRank = 0;
        for (const Team &yesterdayTeam : _yesterdaysTeams)
        {
            currentRank++;
            if (yesterdayTeam.teamId == team.teamId)
            {
                deltaRank = currentRank - (rank - 1);
                deltaGames = team.games - yesterdayTeam.games;
                deltaDiff = team.eloDifference - yesterdayTeam.eloDifference;
                break;
            }
        }


        jTeam["delta_rank"] = deltaRank;
        jTeam["delta_gamesplayed"] = deltaGames;
        jTeam["delta_diff"] = deltaDiff;

        jTeams.push_back(jTeam);

        if (rank == 31)
            break;
    }

    data["data"] = jTeams;

    if (!_teams.empty())
    {
        std::string filename = gamemodes::shortName(_gameMode) + "_best_teams.json";
        std::ofstream stream(directory / filename);
        stream << std::setw(4) << data << std::endl;
        stream.close();
    }

    Log::verbose() << "Exported best teams.";
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
    Log::verbose() << "Exported biggest upsets of the last 12 month.";

    exportUpsets(directory, _upsetsLast30Days, gamemodes::shortName(_gameMode) + "_upsets_last30days.json", "Upsets within the last 30 days", players);
    Log::verbose() << "Exported biggest upsets of the last 30 days.";

    exportUpsets(directory, _upsetsAllTime, gamemodes::shortName(_gameMode) + "_upsets_alltime.json", "Biggest upsets of all time", players);
    Log::verbose() << "Exported biggest upsets of all time..";
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
        { { "index", 7 }, { "header", "Diff" } , { "name", "rating_difference" }, { "info", "Difference in ELO rating. Considers deviation."} }
    });

    json jUpsets = json::array();
    int rank = 1;

    for (auto it = upsets.begin(); it != upsets.end(); ++it)
    {
        json jUpset = json::object();
        jUpset["rank"] = rank++;
        jUpset["date"] = stringtools::fromDate(it->date);
        jUpset["winner"] =
            players[it->winners[0]].alias() + ((it->winners.size() == 1) ? "" :
            " (" + std::to_string(it->winnerElo[0]) + ") / "
            + players[it->winners[1]].alias() + " (" + std::to_string(it->winnerElo[1]) + ")");

        jUpset["loser"] =
            players[it->losers[0]].alias() + ((it->losers.size() == 1) ? "" :
            " (" + std::to_string(it->loserElo[0]) + ") / "
            + players[it->losers[1]].alias() + " (" + std::to_string(it->loserElo[1]) + ")");

        jUpset["faction_winner"] = factions::shortName(it->winnerFaction());
        jUpset["faction_loser"] = factions::shortName(it->loserFaction());
        jUpset["map"] = it->map;
        jUpset["rating_difference"] = "\u2265 " + std::to_string(static_cast<int>(it->eloDifference));
        jUpsets.push_back(jUpset);
    }

    data["data"] = jUpsets;

    std::ofstream stream(directory / filename);
    stream << std::setw(4) << data << std::endl;
    stream.close();
}
