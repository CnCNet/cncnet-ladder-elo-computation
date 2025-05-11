
#include <iostream>
#include <optional>
#include <fstream>

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <nlohmann/json.hpp>

#include "logging.h"
#include "game.h"
#include "stringtools.h"

std::map<std::string, std::map<uint32_t, Game>> gamesByLadder;
std::map<uint32_t, std::string> userToAlias;
std::map<std::string, std::map<std::string, uint32_t>> nickToUserId;

bool loadUser(const std::string &ladder, const std::string &name, sql::Connection* connection)
{
    /* -- Nicks for all ladders:
    std::unique_ptr<sql::PreparedStatement> stmt(
        connection->prepareStatement(
            "SELECT players.user_id, users.alias, ladders.abbreviation, players.username "
            "FROM players "
            "JOIN ladders ON players.ladder_id = ladders.id "
            "JOIN users ON users.id = players.user_id "
            "WHERE players.user_id = ("
            "  SELECT user_id FROM players p2 "
            "  JOIN ladders l2 ON p2.ladder_id = l2.id "
            "  WHERE p2.username = ? AND l2.abbreviation = ? "
            "  LIMIT 1"
            ") ORDER BY ladders.abbreviation"
            )
        );
    */

    std::unique_ptr<sql::PreparedStatement> stmt(
        connection->prepareStatement(
            "SELECT players.user_id, players.username, ladders.abbreviation, users.alias "
            "FROM players "
            "JOIN ladders ON players.ladder_id = ladders.id "
            "JOIN users ON players.user_id = users.id "
            "WHERE players.user_id = ( "
            "  SELECT user_id "
            "  FROM players p2 "
            "  JOIN ladders l2 ON p2.ladder_id = l2.id "
            "  WHERE p2.username = ? AND l2.abbreviation = ? "
            "  LIMIT 1 "
            ") "
            "AND ladders.abbreviation = ? "
            "ORDER BY players.username; "
        )
    );

    stmt->setString(1, name);
    stmt->setString(2, ladder);
    stmt->setString(3, ladder);
    std::unique_ptr<sql::ResultSet> result(stmt->executeQuery());

    bool isEmpty = true;

    while (result->next())
    {
        uint32_t userId = result->getInt("user_id");

        if (isEmpty)
        {
            isEmpty = false;
            std::string alias = result->getString("alias");
            userToAlias[userId] = alias;
        }

        std::string nick = result->getString("username");
        nickToUserId[ladder][nick] = userId;
    }

    return !isEmpty;
}


void fetchRecentGamesByLadder(
    const std::string& ladder,
    sql::Connection* connection,
    std::map<uint32_t, Game> &games)
{
    std::unique_ptr<sql::PreparedStatement> stmt(
        connection->prepareStatement(
            "SELECT "
            "  games.id AS gameId, "
            "  players.username AS playerUsername, "
            "  player_game_reports.won AS playerWon, "
            "  player_game_reports.points, "
            "  sides.name AS playerCountry, "
            "  COALESCE(maps.name, games.scen) AS map, "
            "  game_reports.duration, "
            "  game_reports.fps, "
            "  UNIX_TIMESTAMP(games.updated_at) AS timestamp, "
            "  games.updated_at AS played "
            "FROM games "
            "JOIN ladder_history ON games.ladder_history_id = ladder_history.id "
            "JOIN ladders ON ladder_history.ladder_id = ladders.id "
            "JOIN game_reports ON game_reports.id = games.game_report_id "
            "JOIN player_game_reports ON player_game_reports.game_report_id = games.game_report_id "
            "JOIN players ON players.id = player_game_reports.player_id "
            "JOIN stats2 ON stats2.id = player_game_reports.stats_id "
            "LEFT JOIN sides ON sides.local_id = stats2.cty AND sides.ladder_id = ladders.id "
            "LEFT JOIN qm_matches qmm ON qmm.id = games.qm_match_id "
            "LEFT JOIN qm_maps qmap ON qmm.qm_map_id = qmap.id "
            "LEFT JOIN maps maps ON maps.id = qmap.map_id "
            "WHERE ladders.abbreviation = ? "
            "ORDER BY games.updated_at ASC "
        )
    );

    stmt->setString(1, ladder);

    std::unique_ptr<sql::ResultSet> result(stmt->executeQuery());

    while (result->next())
    {
        uint32_t gameId = static_cast<uint32_t>(result->getInt("gameId"));

        if (!games.contains(gameId))
        {
            uint32_t fps = static_cast<uint32_t>(result->getInt("fps"));
            uint32_t duration = static_cast<uint32_t>(result->getInt("duration"));
            uint32_t timestamp = static_cast<uint32_t>(result->getInt64("timestamp"));
            std::string map = result->getString("map");
            games.emplace(gameId, Game(gameId, map, timestamp, fps, duration));
        }

        auto it = games.find(gameId);
        Game *game = (it != games.end()) ? &(it->second) : nullptr;
        if (game == nullptr)
            continue;

        std::string playerName = result->getString("playerUsername");
        // TODO: Consider points to determine winner/loser in undecided games.
        // int32_t points = result->getInt("points");
        bool won = result->getBoolean("playerWon");
        std::string playerCountry = result->getString("playerCountry");

        // Get user id by playername.
        if (!nickToUserId[ladder].contains(playerName))
        {
            bool success = loadUser(ladder, playerName, connection);
            if (!success)
            {
                Log::fatal() << "Unable to determine user from playername '" << playerName << "'.";
                continue;
            }
        }

        assert(nickToUserId[ladder].contains(playerName));
        uint32_t userId = nickToUserId[ladder][playerName];
        game->addPlayer(userId, playerName, factions::fromName(playerCountry), won, 0.0, 0.0);
    }
}

int main()
{
    try
    {
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
        std::unique_ptr<sql::Connection> con(
            //driver->connect("tcp://127.0.0.1:3307", "cncnet", "cncnet")
            driver->connect("tcp://host.docker.internal:3307", "cncnet", "cncnet")
        );
        con->setSchema("cncnet_api");

        fetchRecentGamesByLadder("blitz", con.get(), gamesByLadder["blitz"]);

        Log::info() << "Fetched " << gamesByLadder["blitz"].size() << " blitz games.";
        Log::info() << "Fetched " << userToAlias.size() << " individual blitz players.";

        nlohmann::json testdata;
        testdata["blitzgames"] = gamesByLadder["blitz"].size();
        testdata["blitzplayers"] = userToAlias.size();

        // Write json test file.
        std::ofstream file("/data/test.json");
        if (file.is_open())
        {
            file << testdata.dump(4);
            file.close();
        }
        else
        {
            Log::error() << "Unable to write json test file.";
        }

    }
    catch (sql::SQLException &e)
    {
        Log::fatal() << "SQL: " << e.what();
        return 1;
    }

    return 0;
}
