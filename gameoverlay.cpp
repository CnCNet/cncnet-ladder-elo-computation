
#include <cstdint>
#include <fstream>

#include <nlohmann/json.hpp>

#include "blitzmap.h"
#include "faction.h"
#include "game.h"
#include "gameoverlay.h"
#include "logging.h"

/*!
 */
void GameOverlay::loadTournamentGames(
    DatabaseConnection &connection,
    const std::filesystem::path &file,
    Players &players,
    gamemodes::GameMode gameMode,
    const std::string &ladderAbbreviation,
    std::map<uint32_t, Game> &games
    )
{
    static int tournamentGameNumber = 100000000;
    static int currentFakeUserId    = 100000000;

    std::ifstream f(file);
    using json = nlohmann::json;
    json data = json::parse(f);

    std::map<uint32_t, uint32_t> timeAdditions;

    for (auto& jsonGames : data)
    {
        if (!jsonGames.is_object())
        {
            continue;
        }

        if (!jsonGames.contains("map") || !jsonGames.contains("date") || !jsonGames.contains("games"))
        {
            Log::error() << "No map, date or games in tournament file.";
            continue;
        }

        int mapIndex = blitzmap::toIndex(jsonGames["map"].get<std::string>());
        if (mapIndex < 0)
        {
            Log::error() << "Unknown map '" << jsonGames["map"].get<std::string>() << "'.";
            continue;
        }

        uint32_t datex = static_cast<uint32_t>(std::stoul(jsonGames["date"].get<std::string>()));

        json gamesNode = jsonGames["games"];
        for (auto& jsonGame : gamesNode)
        {
            if (!timeAdditions.contains(datex))
            {
                timeAdditions[datex] = 1;
            }
            else
            {
                timeAdditions[datex] += 5;
            }

            int year = datex / 10000;
            unsigned month = (datex / 100) % 100;
            unsigned day = datex % 100;

            std::chrono::year_month_day ymd{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
            std::chrono::sys_days sysDays = std::chrono::sys_days{ymd};

            // We do not have a specific timestamp and just assume the games start at 8pm UTC. The duration
            // is set to 0 and will be ignored in any kind of stats which use game times.
            std::chrono::sys_seconds dateTimeAt20 = std::chrono::sys_seconds{sysDays} + std::chrono::hours{20};
            std::chrono::sys_seconds finalTime = dateTimeAt20 + std::chrono::seconds{60 * timeAdditions[datex]};
            std::time_t unixTimestamp = std::chrono::system_clock::to_time_t(finalTime);
            uint32_t timestamp = static_cast<uint32_t>(unixTimestamp);

            if (!jsonGame.contains("p1") || !jsonGame.contains("p1")
                || !jsonGame.contains("p1") || !jsonGame.contains("p1") || !jsonGame.contains("result"))
            {
                Log::error() << "Bad tournament game format.";
                continue;
            }

            std::string playerAlias1 = jsonGame["p1"].get<std::string>();
            std::string playerAlias2 = jsonGame["p2"].get<std::string>();
            factions::Faction faction1 = (jsonGame["f1"].get<std::string>() == std::string("a")) ? factions::Allied : factions::Soviet;
            factions::Faction faction2 = (jsonGame["f2"].get<std::string>() == std::string("a")) ? factions::Allied : factions::Soviet;

            uint32_t userId1 = players.userIdFromAlias(playerAlias1);
            if (userId1 == 0)
            {
                userId1 = connection.loadPlayerFromAlias(playerAlias1, players);
                if (userId1 == 0)
                {
                    // Need to create a player with a fake id.
                    userId1 = currentFakeUserId++;
                    Player player(userId1, 0, "?", gameMode);
                    Log::info() << "Manually created player " << userId1 << " with alias " << playerAlias1 << ".";
                    player.setAlias(playerAlias1);
                    players.add(player, ladderAbbreviation);
                }
            }

            const Player &player1 = players[userId1];

            uint32_t userId2 = players.userIdFromAlias(playerAlias2);
            if (userId2 == 0)
            {
                userId2 = connection.loadPlayerFromAlias(playerAlias2, players);
                if (userId2 == 0)
                {
                    // Need to create a player with a fake id.
                    userId2 = currentFakeUserId++;
                    Player player(userId2, 0, "?", gameMode);
                    player.setAlias(playerAlias2);
                    Log::info() << "Manually created player " << userId2 << " with alias " << playerAlias2 << ".";
                    players.add(player, ladderAbbreviation);
                }
            }

            const Player &player2 = players[userId2];

            int result = jsonGame["result"].get<int>();

            if (player1.userId() == 0 || player2.userId() == 0)
            {
                Log::error() << "Skipping tournament game " << playerAlias1 << " (" << player1.userId() << ") vs "
                             << playerAlias2 << " (" << player2.userId() << "), because players are unknown.";
                continue;
            }

            std::pair<std::map<uint32_t, Game>::iterator, bool> finalGame =
                games.emplace(tournamentGameNumber++, Game(tournamentGameNumber++, blitzmap::__shortNames[mapIndex], timestamp, 0, 0));

            finalGame.first->second.setGameType(gametypes::WorldSeries);
            finalGame.first->second.setLadderAbbreviation(ladderAbbreviation);
            finalGame.first->second.addPlayer(player1.userId(), playerAlias1, faction1, result == 1, 0, 0.0, 0.0);
            finalGame.first->second.addPlayer(player2.userId(), playerAlias2, faction2, result == 2, 0, 0.0, 0.0);
            if (result == 0)
            {
                finalGame.first->second.setIsDraw(true);
            }

            Log::info() << "Added tournament game: " << finalGame.first->second;

        } // for (auto& game : games)

    } // for (auto& jsonGames : data)
}
