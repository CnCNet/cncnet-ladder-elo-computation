 
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "nlohmann/json.hpp"

#include "cplusplus.h"
#include "knownplayers.h"
#include "logging.h"
#include "players.h"
#include "stringtools.h"

/*!
 */
Players::Players()
{
    // TODO: Mark test accounts in the database.
    // Games with test account don't count for ELO.
    _testAccounts.insert({59825, 69266, 75413, 75411, 75636, 11533, 12934, 59854, 60320, 60348, 60366, 63387, 69268, 76947 });
}

/*!
 */
size_t Players::count() const
{
    return _players.size();
}

/*!
 */
bool Players::contains(uint32_t userId) const
{
    return _players.contains(userId);
}

/*!
 */
bool Players::exists(const std::string &playerName)
{
    return _nickToUserId.contains(playerName);
}

/*!
 */
std::vector<uint32_t> Players::userIds() const
{
    std::vector<uint32_t> userIds;
    userIds.reserve(_players.size());

    for (const auto& [userId, player] : _players)
    {
        userIds.push_back(userId);
    }

    return userIds;
}

/*!
 */
Player& Players::operator[](uint32_t index)
{
    if (index == 0)
    {
        Log::error() << "Player with user id 0 is not supposed to exist.";
    }

    if (!_players.contains(index))
    {
        Log::fatal() << "No player for index " << index << ".";
        throw std::runtime_error("Accessing non-existing player.");
    }

    return _players[index];
}

/*!
 */
Player Players::operator[](uint32_t index) const
{
    if (index == 0)
    {
        Log::error() << "Player with user id 0 is not supposed to exist.";
    }

    Log::fatal(!_players.contains(index)) << "No player for index " << index << ". An exception will be thrown.";

    return _players.at(index);
}

/*!
 */
void Players::add(const Player &player, const std::string &ladderAbbreviation)
{
    if (_players.contains(player.userId()))
    {
        Log::error() << "User id " << player.userId() << " already exists.";
    }

    _players[player.userId()] = player;

    const std::map<std::string, std::set<std::string>> &names = player.names();

    for (auto it = names.begin(); it != names.end(); ++it)
    {
        for (const std::string &name : it->second)
        {
            _nickToUserId[it->first][name] = player.userId();
        }
    }
}

/*!
 */
void Players::markDuplicates(uint32_t id, const std::set<uint32_t> &duplicates)
{
    for (auto it = _nickToUserId.begin(); it != _nickToUserId.end(); ++it)
    {
        std::map<std::string, uint32_t> &innerMap = it->second;

        for (auto innerIt = innerMap.begin(); innerIt != innerMap.end(); ++innerIt)
        {
            if (duplicates.contains(innerIt->second))
            {
                innerIt->second = id;
            }
        }
    }
}

/*!
 */
bool Players::isTestAccount(uint32_t userId) const
{
    return _testAccounts.contains(userId);
}

/*!
 */
uint32_t Players::userId(const std::string &playerName, const std::string &ladderName) const
{
    auto innerIt = _nickToUserId.find(ladderName);
    if (innerIt == _nickToUserId.end())
    {
        return 0;
    }

    auto it = _nickToUserId.at(ladderName).find(playerName);
    return (it != _nickToUserId.at(ladderName).end()) ? it->second : 0;
}


/*!
 */
void Players::update()
{
    for (auto it = _players.begin(); it != _players.end(); ++it)
    {
        Player &player = it->second;

        if (player.gameCount() == 0 && player.pendingGameCount() == 0)
        {
            continue;
        }

        player.update();
    }
}

/*!
 */
bool Players::hasPendingGames() const
{
    bool hasPendingGames = false;

    for (std::map<uint32_t, Player>::const_iterator it = _players.begin(); it != _players.end() && !hasPendingGames; ++it)
    {
        hasPendingGames |= (it->second.pendingGameCount() > 0);
    }

    return hasPendingGames;
}

/*!
 */
void Players::apply(std::chrono::year_month_day date, bool decay, gamemodes::GameMode gameMode)
{
    for (std::map<uint32_t, Player>::iterator it = _players.begin(); it != _players.end(); ++it)
    {
        Player &player = it->second;
        player.apply(date, decay, gameMode);
        if (it->first == dts::to_underlying(KnownPlayers::BlitzBot))
        {
            Log::debug() << "The blitz bots current rating is " << it->second.elo(factions::Combined) << ".";
        }
    }
}

/*!
 */
uint32_t Players::activePlayerCount() const
{
    uint32_t result = 0;

    for (std::map<uint32_t, Player>::const_iterator it = _players.begin(); it != _players.end(); ++it)
    {
        result += it->second.isActive() ? 1 : 0;
    }

    return result;
}

/*!
 */
std::map<uint32_t, uint32_t> Players::exportActivePlayers(const std::filesystem::path &directory, gamemodes::GameMode gameMode) const
{
    Log::info() << "Exporting list of active players.";

    std::map<uint32_t, uint32_t> rankByUserId;
    using json = nlohmann::json;
    json data = json::object();

    std::string yuri = (gameMode == gamemodes::YurisRevenge) ? ", yuri" : "";

    if (gameMode != gamemodes::Blitz2v2)
    {
        data["description"] = "Active players sorted by ELO";
    }
    else
    {
        data["description"] = "Active players sorted by combined ELO";
    }

    data["columns"] = json::array({
        { { "index", 0 }, { "header", "#" } , { "name", "rank" } },
        { { "index", 1 }, { "header", "∆ #" } , { "name", "delta_rank" } },
        { { "index", 2 }, { "header", "" } , { "name", "faction" }, { "info", "Indicating if your allied, soviet" + yuri + " or combined rating is your best." } },
        { { "index", 3 }, { "header", "Name" } , { "name", "name" } },
        { { "index", 4 }, { "header", "Elo" } , { "name", "elo" }, { "info", "Your current ELO." } },
        { { "index", 5 }, { "header", "∆ Elo" } , { "name", "delta_elo" }, { "info", "ELO change since the day before." } },
        { { "index", 6 }, { "header", "Deviation" } , { "name", "deviation" }, { "info", "Your current deviation. The lower the deviation the more accurate is your rating. You need to have a deviation less than about 100 to be considered an active player. The deviation grows if you don't play." } },
        { { "index", 7 }, { "header", "Games" } , { "name", "game_count" }, { "info", "Total number of games played." } },
        { { "index", 8 }, { "header", "∆ Games" } , { "name", "game_diff" }, { "info", "Games played yesterday." } },
        { { "index", 9 }, { "header", "DTI" } , { "name", "days_to_inactivity" }, { "info", "Days to inactivity. If you stop playing today, you will be considered an inactive player after this amount of days." } },
        { { "index", 10 }, { "header", "Elo/Dev." } , { "name", "sov_elo_deviation" } },
        { { "index", 11 }, { "header", "Games" } , { "name", "sov_games" } },
        { { "index", 12 }, { "header", "Elo/Dev." } , { "name", "all_elo_deviation" } },
        { { "index", 13 }, { "header", "Games" } , { "name", "all_games" } }
    });

    if (gameMode == gamemodes::YurisRevenge)
    {
        data["columns"].push_back({ { "index", 14 }, { "header", "Elo/Deviation" } , { "name", "yur_elo_deviation" } });
        data["columns"].push_back({ { "index", 15 }, { "header", "Games" } , { "name", "yur_games" } });
    }

    std::vector<const Player*> filteredAndSortedPlayers;
    std::vector<const Player*> filteredAndSortedPlayersYesterday;

    for (auto it = _players.cbegin(); it != _players.cend(); ++it)
    {
        if (it->second.isActive())
        {
            filteredAndSortedPlayers.push_back(&(it->second));
            filteredAndSortedPlayersYesterday.push_back(&(it->second));
        }
    }

    // Now sort the players.
    if (gameMode == gamemodes::Blitz2v2)
    {
        std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(),
                  [] (const Player *a, const Player *b) { return a->elo(factions::Combined) > b->elo(factions::Combined);});
        std::sort(filteredAndSortedPlayersYesterday.begin(), filteredAndSortedPlayersYesterday.end(),
                  [] (const Player *a, const Player *b) { return a->yesterdaysElo(factions::Combined) > b->yesterdaysElo(factions::Combined);});
    }
    else
    {
        std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(),
                  [] (const Player *a, const Player *b) { return a->maxRating() > b->maxRating();});
        std::sort(filteredAndSortedPlayersYesterday.begin(), filteredAndSortedPlayersYesterday.end(),
                  [] (const Player *a, const Player *b) { return a->yesterdaysMaxRating() > b->yesterdaysMaxRating();});
    }

    json players = json::array();

    for (size_t i = 0; i < filteredAndSortedPlayers.size(); i++)
    {
        json jsonPlayer = json::object();
        const Player *player = filteredAndSortedPlayers[i];

        // Now look for this particular player in the other vector.
        int rankYesterday = 0;
        for (size_t j = 0; j < filteredAndSortedPlayersYesterday.size(); j++)
        {
            if (filteredAndSortedPlayersYesterday[j] == player)
            {
                rankYesterday = static_cast<int>(j + 1);
                break;
            }
        }

        jsonPlayer["rank"] = i + 1;
        jsonPlayer["delta_rank"] = rankYesterday - static_cast<int>(i + 1);
        jsonPlayer["name"] = player->alias();

        factions::Faction faction = (gameMode == gamemodes::Blitz2v2) ? factions::Combined : player->getBestActiveFaction();
        jsonPlayer["faction"] = factions::shortName(faction);

        std::ostringstream oss;
        if (gameMode == gamemodes::Blitz2v2)
        {
            jsonPlayer["elo"] = static_cast<int>(player->elo(factions::Combined));
            jsonPlayer["delta_elo"] = static_cast<int>(player->elo(factions::Combined) - player->yesterdaysElo(factions::Combined));
            oss << std::fixed << std::setprecision(1) << player->deviation(factions::Combined);
        }
        else
        {
            jsonPlayer["elo"] = static_cast<int>(player->elo(faction));
            jsonPlayer["delta_elo"] = static_cast<int>(player->elo(faction) - player->yesterdaysElo(faction));
            oss << std::fixed << std::setprecision(1) << player->deviation(faction);
        }

        jsonPlayer["deviation"] = oss.str();
        jsonPlayer["days_to_inactivity"] = player->daysToInactivity(gameMode);
        jsonPlayer["game_count"] = player->gameCount();
        jsonPlayer["game_diff"] = player->gameCount() - player->yesterdaysGameCount();
        jsonPlayer["active"] = player->isActive();

        PeakRating peak = player->peakRating();

        auto today = floor<std::chrono::days>(std::chrono::system_clock::now());
        auto peakSysDays = std::chrono::sys_days(peak.date);
        auto diff = std::chrono::duration_cast<std::chrono::days>(today - peakSysDays).count();

        if (peak.adjustedElo > 0.0 && diff < 31)
        {
            jsonPlayer["on_fire"] = 1;
        }

        for (int j = 0; j < factions::count(); j++)
        {
            factions::Faction faction = factions::toFaction(j);

            if (faction == factions::Combined)
            {
                // Do not export combined ELO.
                continue;
            }

            if (player->gameCount(faction) > 0)
            {
                std::ostringstream oss;
                oss << static_cast<int>(player->elo(faction)) << " ± " << std::fixed << std::setprecision(1) << player->deviation(faction);
                jsonPlayer[factions::shortName(faction) + "_elo_deviation"] = oss.str();
                jsonPlayer[factions::shortName(faction) + "_games"] = player->gameCount(faction);
            }
        }

        players.push_back(jsonPlayer);
        rankByUserId[player->userId()] = i + 1;
    }

    data["data"] = players;

    std::ofstream stream(directory / (gamemodes::shortName(gameMode) + "_active_players.json"));
    stream << std::setw(4) << data << std::endl;
    stream.close();

    return rankByUserId;
}

/*!
 */
std::map<uint32_t, uint32_t> Players::exportBestOfAllTime(
    const std::filesystem::path &directory,
    gamemodes::GameMode gameMode) const
{
    Log::info() << "Exporting best players of all time.";

    std::vector<const Player*> filteredAndSortedPlayers;
    std::map<uint32_t, uint32_t> rankByUserId;

    for (auto it = _players.cbegin(); it != _players.cend(); ++it)
    {
        const Player &player = it->second;
        PeakRating peak = (gameMode == gamemodes::Blitz2v2) ? player.peakRating(factions::Combined) : player.peakRating();
        if (peak.adjustedElo > 0.0)
        {
            filteredAndSortedPlayers.push_back(&player);
        }
    }

    // Now sort the players.
    if (gameMode == gamemodes::Blitz2v2)
    {
        std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(),
                  [] (const Player *a, const Player *b) { return a->peakRating(factions::Combined).adjustedElo > b->peakRating(factions::Combined).adjustedElo; });
    }
    else
    {
        std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(),
                  [] (const Player *a, const Player *b) { return a->peakRating().adjustedElo > b->peakRating().adjustedElo; });
    }

    using json = nlohmann::json;

    json data = json::object();

    std::string yuri = (gameMode == gamemodes::YurisRevenge) ? ", yuri" : "";

    data["description"] = "Best ranked match players of all time";
    data["columns"] = json::array({
        { { "index", 0 }, { "header", "#" } , { "name", "rank" } },
        { { "index", 1 }, { "header", "" } , { "name", "faction" }, { "info", "Indicating if your allied, soviet" + yuri + " or combined rating is used." } },
        { { "index", 2 }, { "header", "Name" } , { "name", "name" } },
        { { "index", 3 }, { "header", "Date" } , { "name", "date" }, { "info", "The date on which the peak was reached." } },
        { { "index", 4 }, { "header", "Peak" } , { "name", "peak" }, { "info", "Peak rating is ELO minus deviation at the given point of time." } },
        { { "index", 5 }, { "header", "Status" } , { "name", "status" } }
    });

    json players = json::array();

    for (int i = 0; i < filteredAndSortedPlayers.size(); i++)
    {
        json jsonPlayer = json::object();

        const Player *player = filteredAndSortedPlayers[i];

        PeakRating peak = (gameMode == gamemodes::Blitz2v2) ? player->peakRating(factions::Combined) : player->peakRating();

        jsonPlayer["rank"] = i + 1;
        jsonPlayer["name"] = player->alias();
        jsonPlayer["faction"] = factions::shortName(peak.faction);
        jsonPlayer["peak"] = std::to_string(static_cast<int>(peak.adjustedElo));
        jsonPlayer["deviation"] = std::to_string(static_cast<int>(peak.deviation));
        jsonPlayer["date"] = stringtools::fromDate(peak.date);
        if (player->isActive() && player->maxRating(false) > 0)
        {
            jsonPlayer["current"] = std::to_string(static_cast<int>(player->maxRating(false)));
        }
        jsonPlayer["game_count"] = player->gameCount();
        jsonPlayer["status"] = player->isActive() ? "ACTIVE" : "INACTIVE";

        players.push_back(jsonPlayer);
        rankByUserId[player->userId()] = i + 1;
    }
    data["data"] = players;

    std::ofstream stream(directory / (gamemodes::shortName(gameMode) + "_bestofalltime.json"));
    stream << std::setw(4) << data << std::endl;
    stream.close();

    return rankByUserId;
}

/*!
 */
void Players::exportMostDaysActive(
    const std::filesystem::path &directory,
    gamemodes::GameMode gameMode) const
{
    Log::info() << "Exporting most loyal players.";

    std::vector<const Player*> filteredAndSortedPlayers;

    for (auto it = _players.cbegin(); it != _players.cend(); ++it)
    {
        const Player &player = it->second;
        if (player.daysActive() > 0)
        {
            filteredAndSortedPlayers.push_back(&player);
        }
    }

    // Now sort the players.
    std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(),
              [] (const Player *a, const Player *b) { return a->daysActive() > b->daysActive(); });

    using json = nlohmann::json;

    json data = json::object();

    data["description"] = "Days being an active player since ranked match start in July 2022";
    data["columns"] = json::array({
        { { "index", 0 }, { "header", "#" } , { "name", "rank" } },
        { { "index", 1 }, { "header", "Name" } , { "name", "name" } },
        { { "index", 2 }, { "header", "First game" } , { "name", "first_game" } },
        { { "index", 3 }, { "header", "Last game" } , { "name", "last_game" } },
        { { "index", 4 }, { "header", "Days active" } , { "name", "days" } }
    });

    json players = json::array();

    // Maximum 30 players.
    for (size_t i = 0; i < filteredAndSortedPlayers.size() && i < 30U; i++)
    {
        json jsonPlayer = json::object();

        const Player *player = filteredAndSortedPlayers[i];

        jsonPlayer["rank"] = i + 1;
        jsonPlayer["name"] = player->alias();
        jsonPlayer["first_game"] = stringtools::fromDate(player->firstGame());
        jsonPlayer["last_game"] = stringtools::fromDate(player->lastGame());
        jsonPlayer["days"] = player->daysActive();

        players.push_back(jsonPlayer);
    }
    data["data"] = players;

    std::ofstream stream(directory / (gamemodes::shortName(gameMode) + "_daysactive.json"));
    stream << std::setw(4) << data << std::endl;
    stream.close();
}

/*!
 */
void Players::exportAlphabeticalOrder(
    const std::filesystem::path &directory,
    gamemodes::GameMode gameMode) const
{
    Log::info() << "Export all active and formerly active players in alphabetical order.";

    using json = nlohmann::json;
    json data = json::object();

    data["description"] = "All players, who are or were active, in alphabetical order";
    data["columns"] = json::array({
        { { "index", 0 }, { "header", "Name" } , { "name", "name" } },
        { { "index", 1 }, { "header", "Status" } , { "name", "status" } },
        { { "index", 2 }, { "header", "Last game" } , { "name", "date" } },
        { { "index", 3 }, { "header", "Games" } , { "name", "mix_games" }, { "info", "Total number of games played." } },
        { { "index", 4 }, { "header", "Elo" } , { "name", "sov_elo" } },
        { { "index", 5 }, { "header", "Games" } , { "name", "sov_games" } },
        { { "index", 6 }, { "header", "Elo" } , { "name", "all_elo" } },
        { { "index", 7 }, { "header", "Games" } , { "name", "all_games" } },
        { { "index", 8 }, { "header", "Elo" } , { "name", "mix_elo" } }
    });

    std::vector<const Player*> filteredAndSortedPlayers;

    auto sorter =  [] (const Player *a, const Player *b) { return a->lowerLexicalOrder(*b); };

    for (auto it = _players.cbegin(); it != _players.cend(); ++it)
    {
        if (it->second.wasActive())
        {
            filteredAndSortedPlayers.push_back(&(it->second));
        }
    }

    // Now sort the players.
    std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(), sorter);

    json players = json::array();

    for (int i = 0; i < filteredAndSortedPlayers.size(); i++)
    {
        json jsonPlayer = json::object();

        const Player *player = filteredAndSortedPlayers[i];

        jsonPlayer["name"] = player->alias();
        jsonPlayer["status"] = player->isActive() ? "ACTIVE" : "INACTIVE";
        jsonPlayer["date"] = stringtools::fromDate(player->lastGame());

        PeakRating peak = player->peakRating();

        auto today = floor<std::chrono::days>(std::chrono::system_clock::now());
        auto peakSysDays = std::chrono::sys_days(peak.date);
        auto diff = std::chrono::duration_cast<std::chrono::days>(today - peakSysDays).count();

        if (peak.adjustedElo > 0.0 && diff < 31)
        {
            jsonPlayer["on_fire"] = 1;
        }

        for (int j = 0; j < factions::count(); j++)
        {
            factions::Faction faction = factions::toFaction(j);

            if (player->gameCount(faction) > 0)
            {
                jsonPlayer[factions::shortName(faction) + "_elo"] = std::to_string(static_cast<int>(player->elo(faction)));
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(1) << player->deviation(faction);
                jsonPlayer["deviation"] = oss.str();
                jsonPlayer[factions::shortName(faction) + "_games"] = player->gameCount(faction);
            }
        }

        players.push_back(jsonPlayer);
    }

    data["data"] = players;

    std::ofstream stream(directory / (gamemodes::shortName(gameMode) + "_players_all_alphabetical.json"));
    stream << std::setw(4) << data << std::endl;
    stream.close();
}

/*!
 */
void Players::exportAllPlayers(
    const std::filesystem::path &directory,
    gamemodes::GameMode gameMode) const
{
    Log::info() << "Export all players in the order of their ids.";

    using json = nlohmann::json;
    json data = json::object();

    data["description"] = "All players, who are or were active, in alphabetical order";
    data["columns"] = json::array({
        { { "index", 0 }, { "header", "Name" } , { "name", "name" } },
        { { "index", 1 }, { "header", "Status" } , { "name", "status" } },
        { { "index", 2 }, { "header", "Last game" } , { "name", "date" } },
        { { "index", 3 }, { "header", "Games" } , { "name", "mix_games" }, { "info", "Total number of games played." } },
        { { "index", 4 }, { "header", "Elo" } , { "name", "sov_elo" } },
        { { "index", 5 }, { "header", "Games" } , { "name", "sov_games" } },
        { { "index", 6 }, { "header", "Elo" } , { "name", "all_elo" } },
        { { "index", 7 }, { "header", "Games" } , { "name", "all_games" } },
        { { "index", 8 }, { "header", "Elo" } , { "name", "mix_elo" } }
    });

    std::vector<const Player*> filteredAndSortedPlayers;

    auto sorter =  [] (const Player *a, const Player *b) { return a->userId() < b->userId(); };

    for (auto it = _players.cbegin(); it != _players.cend(); ++it)
    {
        filteredAndSortedPlayers.push_back(&(it->second));
    }

    // Now sort the players.
    std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(), sorter);

    json players = json::array();

    for (int i = 0; i < filteredAndSortedPlayers.size(); i++)
    {
        json jsonPlayer = json::object();

        const Player *player = filteredAndSortedPlayers[i];

        jsonPlayer["name"] = player->alias();
        jsonPlayer["id"] = "#" + std::to_string(player->userId());
        jsonPlayer["status"] = player->isActive() ? "ACTIVE" : "INACTIVE";
        jsonPlayer["date"] = stringtools::fromDate(player->lastGame());

        for (int j = 0; j < factions::count(); j++)
        {
            factions::Faction faction = factions::toFaction(j);

            if (player->gameCount(faction) > 0)
            {
                jsonPlayer[factions::shortName(faction) + "_elo"] = std::to_string(static_cast<int>(player->elo(faction)));
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(1) << player->deviation(faction);
                jsonPlayer["deviation"] = oss.str();
                jsonPlayer[factions::shortName(faction) + "_games"] = player->gameCount(faction);
            }
        }

        players.push_back(jsonPlayer);
    }

    data["data"] = players;

    std::ofstream stream(directory / (gamemodes::shortName(gameMode) + "_all_players.json"));
    stream << std::setw(4) << data << std::endl;
    stream.close();
}

/*!
 */
void Players::exportNewPlayers(const std::filesystem::path &directory, gamemodes::GameMode gameMode) const
{
    Log::info() << "Exporting new players.";

    using json = nlohmann::json;
    json data = json::object();

    data["description"] = "All players, who are not active, but played a game within the last 30 days, in alphabetical order";
    data["columns"] = json::array({
        { { "index", 0 }, { "header", "Name" } , { "name", "name" } },
        { { "index", 1 }, { "header", "Last game" } , { "name", "date" } },
        { { "index", 2 }, { "header", "Elo" } , { "name", "sov_elo" } },
        { { "index", 3 }, { "header", "Deviation" } , { "name", "sov_deviation" } },
        { { "index", 4 }, { "header", "Games" } , { "name", "sov_games" } },
        { { "index", 5 }, { "header", "Elo" } , { "name", "all_elo" } },
        { { "index", 6 }, { "header", "Deviation" } , { "name", "all_deviation" } },
        { { "index", 7 }, { "header", "Games" } , { "name", "all_games" } },
        { { "index", 8 }, { "header", "Elo" } , { "name", "mix_elo" } },
        { { "index", 9 }, { "header", "Deviation" } , { "name", "mix_deviation" } },
        { { "index", 10 }, { "header", "Games" } , { "name", "mix_games" } }
    });

    std::vector<const Player*> filteredAndSortedPlayers;

    auto sorter =  [] (const Player *a, const Player *b) { return a->lowerLexicalOrder(*b); };

    for (auto it = _players.cbegin(); it != _players.cend(); ++it)
    {
        // Need to ask for more than 0 games, otherwise we might get an alt account.
        if (!it->second.isActive() && it->second.gameCount() > 0 && it->second.daysFromLastGame() <= 30)
        {
            filteredAndSortedPlayers.push_back(&(it->second));
        }
    }

    // Now sort the players.
    std::sort(filteredAndSortedPlayers.begin(), filteredAndSortedPlayers.end(), sorter);

    json players = json::array();

    for (int i = 0; i < filteredAndSortedPlayers.size(); i++)
    {
        json jsonPlayer = json::object();

        const Player *player = filteredAndSortedPlayers[i];

        jsonPlayer["name"] = player->alias();
        jsonPlayer["date"] = stringtools::fromDate(player->lastGame());

        for (int j = 0; j < factions::count(); j++)
        {
            factions::Faction faction = factions::toFaction(j);

            if (player->gameCount(faction) > 0)
            {
                jsonPlayer[factions::shortName(faction) + "_elo"] = std::to_string(static_cast<int>(player->elo(faction)));
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(1) << player->deviation(faction);
                jsonPlayer[factions::shortName(faction) + "_deviation"] = oss.str();
                jsonPlayer[factions::shortName(faction) + "_games"] = player->gameCount(faction);
            }
        }

        players.push_back(jsonPlayer);
    }

    data["data"] = players;

    std::ofstream stream(directory / (gamemodes::shortName(gameMode) + "_new_players.json"));
    stream << std::setw(4) << data << std::endl;
    stream.close();
}


/*!
 */
void Players::finalize()
{
    for (std::map<uint32_t, Player>::iterator it = _players.begin(); it != _players.end(); ++it)
    {
        it->second.finalize();
    }
}

/*!
 */
void Players::exportPlayerDetails(
    const std::filesystem::path &directory,
    std::vector<uint32_t> userIds,
    const std::map<uint32_t, Game> &games,
    const std::string &ladderAbbreviation) const
{
    using json = nlohmann::json;

    if (userIds.empty())
    {
        for (const std::pair<uint32_t, Player> &pair : _players)
        {
            if (pair.second.gameCount() > 0)
            {
                userIds.push_back(pair.first);
            }
        }
    }

    for (uint32_t id : userIds)
    {
        if (!_players.contains(id))
        {
            Log::warning() << "Player with id " << id << " not found. Nothing to export.";
            continue;
        }

        const Player &player = _players.at(id);
        json jplayer = json::object();

        jplayer["alias"] = player.alias();
        jplayer["is_active"] = player.isActive();
        jplayer["wins"] = player.wins();
        jplayer["losses"] = player.losses();
        jplayer["draws"] = player.draws();

        json jHighestRatedVictories = json::array();

        // Reverse the list since it's upside down.
        const std::set<HighestRatedVictories>& highestRatedVictories = player.highestRatedVictories();
        std::vector<HighestRatedVictories> hrvs(highestRatedVictories.begin(), highestRatedVictories.end());
        std::reverse(hrvs.begin(), hrvs.end());

        int counter = 1;
        for (const HighestRatedVictories &highestRatedVictory : hrvs)
        {
            if (highestRatedVictory.ratingDifference <= 0.0)
            {
                // We do not wanna have negative numbers for players who didn't beat higher ranked players.
                break;
            }
            const Game &game = games.at(highestRatedVictory.gameId);
            json jGame = json::object();
            jGame["id"] = game.id();
            int us = game.winnerIndex();
            int them = us ^ 1;
            const Player &opponent = _players.at(game.userId(them));
            jGame["faction"] = factions::shortName(game.faction(us));
            jGame["opponent"] = game.userId(them);
            jGame["opponent_alias"] = opponent.alias();
            jGame["opponent_faction"] = factions::shortName(game.faction(them));
            jGame["map"] = game.mapName();
            jGame["diff"] = highestRatedVictory.ratingDifference;
            jGame["date"] = stringtools::fromDate(game.date());
            jGame["rank"] = counter;
            jHighestRatedVictories.push_back(jGame);
            counter++;
        }

        jplayer["highest_rated_victories"] = jHighestRatedVictories;

        json jLowestRatedDefeats = json::array();

        // Reverse the list since it's upside down.
        const std::set<LowestRatedDefeats>& lowestRatedDefeats = player.lowestRatedDefeats();
        std::vector<LowestRatedDefeats> lrds(lowestRatedDefeats.begin(), lowestRatedDefeats.end());
        std::reverse(lrds.begin(), lrds.end());

        counter = 1;
        for (const LowestRatedDefeats &lowestRatedDefeat : lrds)
        {
            if (lowestRatedDefeat.ratingDifference <= 0.0)
            {
                // We do not wanna have negative numbers for players who only lost to better players.
                break;
            }
            const Game &game = games.at(lowestRatedDefeat.gameId);
            json jGame = json::object();
            jGame["id"] = game.id();
            int us = game.winnerIndex() ^ 1;
            int them = us ^ 1;
            const Player &opponent = _players.at(game.userId(them));
            jGame["faction"] = factions::shortName(game.faction(us));
            jGame["opponent"] = game.userId(them);
            jGame["opponent_alias"] = opponent.alias();
            jGame["opponent_faction"] = game.faction(them);
            jGame["map"] = game.mapName();
            jGame["diff"] = lowestRatedDefeat.ratingDifference;
            jGame["date"] = stringtools::fromDate(game.date());
            jGame["rank"] = counter++;
            jLowestRatedDefeats.push_back(jGame);
        }

        jplayer["lowest_rated_defeats"] = jLowestRatedDefeats;

        // ELO over time
        // Peak rating with date.

        json jVs = json::array();

        const std::map<uint32_t, Probabilities>& vsOthers = player.vsOtherPlayers();
        std::set<std::pair<uint32_t, Probabilities>> vsSet;
        for (auto it = vsOthers.cbegin(); it != vsOthers.cend(); ++it)
        {
            vsSet.insert({it->first, it->second});
        }
        std::vector<std::pair<uint32_t, Probabilities>> vsVector(vsSet.begin(), vsSet.end());
        int vsIndex = 1;
        for (std::pair<uint32_t, Probabilities> &probs : vsVector)
        {
            json jopponent = json::object();
            const Player &opponent = _players.at(probs.first);

            // We need to set some rules here:
            // - Minimum 20 games
            // - Opponent was active within the last 180 days
            //   OR opponent is pro and was active within the last year
            if (probs.second.count() >= 20
                && (opponent.isActive() || (opponent.daysInactive() < 180 || (::players::isProPlayer(probs.first) && opponent.daysInactive() < 365))))
            {
                jopponent["index"] = vsIndex++;
                jopponent["alias"] = opponent.alias();
                jopponent["exspected"] = probs.second.expected();
                jopponent["actual"] = probs.second.actual();
                jopponent["value"] = probs.second.result();
                jopponent["wins"] = probs.second.wins();
                jopponent["losses"] = probs.second.losses();
                jopponent["daysinactive"] = opponent.daysInactive();
                jVs.push_back(jopponent);
            }
        }

        jplayer["vs"] = jVs;

        json jFactions = json::array();
        for (int i = 0; i < factions::UnknownSetup; i++)
        {

            json jMaps = json::array();
            std::set<std::pair<uint32_t, const Probabilities&>> setProbs;
            for (int mapIndex = 0; mapIndex < blitzmap::count(); mapIndex++)
            {
                setProbs.insert({ mapIndex, player.mapStats(static_cast<factions::Setup>(i), mapIndex) });
            }
            std::vector<std::pair<uint32_t, Probabilities>> vProbs(setProbs.begin(), setProbs.end());
            int counter = 1;
            for (const std::pair<uint32_t, Probabilities> &mapProbs : vProbs)
            {
                if (mapProbs.second.count() > 12)
                {
                    // 12 games minimum.
                    json jMap = json::object();
                    jMap["name"] = blitzmap::__names[mapProbs.first];
                    jMap["value"] = mapProbs.second.result();
                    jMap["actual"] = mapProbs.second.actual();
                    jMap["expected"] = mapProbs.second.expected();
                    jMap["wins"] = mapProbs.second.wins();
                    jMap["losses"] = mapProbs.second.losses();
                    jMap["rank"] = counter++;
                    jMaps.push_back(jMap);
                }
            }

            if (!jMaps.empty())
            {
                json jFaction = json::object();
                jFaction["faction"] = factions::toString(static_cast<factions::Setup>(i));
                jFaction["maps"] = jMaps;
                jFactions.push_back(jFaction);
            }
        }

        jplayer["mapstats"] = jFactions;

        json jHistoricalElos = json::array();

        for (int i = 0; i < factions::count(); i++)
        {
            std::map<uint32_t, std::pair<double, double>> historicalElo = player.historicalElo(factions::toFaction(i));

            if (!historicalElo.empty())
            {
                json jHistoricalElo = json::object();
                jHistoricalElo["faction"] = factions::shortName(factions::toFaction(i));
                json jHistoricalValues = json::array();
                for (auto it = historicalElo.begin(); it != historicalElo.end(); ++it)
                {
                    int y = it->first / 10000;
                    unsigned m = (it->first / 100) % 100;
                    unsigned d = it->first % 100;
                    std::chrono::year_month_day ymd = std::chrono::year{y} / std::chrono::month{m} / std::chrono::day{d};
                    json jHistoricValue = json::object();
                    jHistoricValue["date"] = stringtools::fromDate(ymd);
                    jHistoricValue["rating"] = it->second.first;
                    jHistoricValue["deviation"] = it->second.second;
                    jHistoricalValues.push_back(jHistoricValue);
                }
                jHistoricalElo["values"] = jHistoricalValues;
                jHistoricalElos.push_back(jHistoricalElo);
            }
        }

        jplayer["historical_elo"] = jHistoricalElos;

        json jPeaks = json::array();
        for (int i = 0; i < factions::count(); i++)
        {
            if (player.wasActive(factions::toFaction(i)))
            {
                PeakRating peak = player.peakRating(factions::toFaction(i));
                if (peak.adjustedElo > 0.0)
                {
                    json jPeak = json::object();
                    jPeak["faction"] = factions::shortName(factions::toFaction(i));
                    jPeak["elo"] = peak.adjustedElo + peak.deviation;
                    jPeak["deviation"] = peak.deviation;
                    jPeak["date"] = stringtools::fromDate(peak.date);
                    jPeaks.push_back(jPeak);
                }
            }
        }

        jplayer["peak_elo"] = jPeaks;

        json jCurrent = json::array();
        for (int i = 0; i < factions::count(); i++)
        {
            if (player.isActive(factions::toFaction(i)))
            {
                json jElo = json::object();
                jElo["faction"] = factions::shortName(factions::toFaction(i));
                jElo["elo"] = player.elo(factions::toFaction(i));
                jElo["deviation"] = player.deviation(factions::toFaction(i));
                jCurrent.push_back(jElo);
            }
        }

        jplayer["current_elo"] = jCurrent;

        std::stringstream ss;
        ss << ladderAbbreviation << "_player_" << id << ".json";
        std::ofstream stream(directory / ss.str());
        stream << std::setw(4) << jplayer << std::endl;
        stream.close();

    } // for (uint32_t id : userIds)
}

/*!
 */
uint32_t Players::userIdFromAlias(const std::string &alias) const
{
    for (auto it = _players.begin(); it != _players.end(); ++it)
    {
        if (it->second.hasAlias() && it->second.alias() == alias)
        {
            return it->first;
        }
    }

    return 0;
}
