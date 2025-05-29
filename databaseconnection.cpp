
#include <queue>

#include "databaseconnection.h"
#include "players.h"
#include "stringtools.h"

/*!
 */
DatabaseConnection::DatabaseConnection(const Options &options) :
    _ready(false),
    _ladder(options.ladderAbbreviation),
    _gameMode(gamemodes::toGameMode(options.ladderAbbreviation)),
    _connection(nullptr),
    _tournamtGamesAdded(!options.tournamentFile.empty())
{
    try
    {
        std::string hostName = "tcp://" + options.mySqlHost() + ":" + std::to_string(options.mySqlPort());
        Log::verbose() << "Trying to establish connection to " << hostName;
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
        _connection = std::unique_ptr<sql::Connection>(
            driver->connect(hostName, options.mySqlUser(), options.mySqlPassword())
        );

        _connection->setSchema("cncnet_api");

        if (_connection->isValid())
        {
            Log::info() << "Connection to cncnet established.";
            _ready = true;
        }
    }
    catch (sql::SQLException &e)
    {
        Log::fatal() << "Unable to establish connection: " << e.what();
    }
}

/*!
 */
bool DatabaseConnection::isEstablished() const
{
    return _ready;
}

/*!
 */
bool DatabaseConnection::ladderExists(const std::string &abbreviation) const
{
    std::unique_ptr<sql::PreparedStatement> statement(
        _connection->prepareStatement(
            "SELECT 1 FROM ladders WHERE abbreviation = ? LIMIT 1"
            )
        );

    statement->setString(1, abbreviation);

    std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
    return result->next();
}

/*!
 */
void DatabaseConnection::initDuplicates()
{
    _duplicates.clear();

    // Get all distinct user ids from table players. The users might have
    // been deleted from the user-table.
    {
        std::unique_ptr<sql::PreparedStatement> statement(
            _connection->prepareStatement("SELECT DISTINCT players.user_id "
                                          "FROM players "
                                          "JOIN ladders ON players.ladder_id = ladders.id "
                                          "JOIN player_game_reports ON player_game_reports.player_id = players.id "
                                          "WHERE ladders.abbreviation = ? "
                                          "UNION "
                                          "SELECT DISTINCT players.user_id "
                                          "FROM players "
                                          "JOIN ladders ON players.ladder_id = ladders.id "
                                          "JOIN users ON players.user_id = users.id "
                                          "WHERE ladders.abbreviation = ? "
                                          "AND users.alias IS NOT NULL "
                                          "AND users.alias != '' "
            )
        );
        statement->setString(1, _ladder);
        statement->setString(2, _ladder);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());

        while (result->next())
        {
            _players.insert(result->getUInt("user_id"));
        }
    }

    Log::info() << "Checking " << _players.size() << " users for duplicates.";

    for (uint32_t uid : _players)
    {
        std::set<uint32_t> duplicates = getWebLikeDuplicateAccounts(uid);
        for (uint32_t duplicate : duplicates)
        {
            _duplicates[uid].insert(duplicate);
            _duplicates[duplicate].insert(uid);

            for (uint32_t x : duplicates)
            {
                if (x != duplicate)
                {
                    _duplicates[duplicate].insert(x);
                }
            }
        }
    }
}

/*!
 */
void DatabaseConnection::addDuplicates(uint32_t userId, std::set<uint32_t> duplicates)
{
    for (auto duplicate : duplicates)
    {
        _duplicates[userId].insert(duplicate);
        _duplicates[duplicate].insert(userId);
    }

    for (auto duplicate : duplicates)
    {
        for (auto innerDuplicate : duplicates)
        {
            if (innerDuplicate != duplicate)
            {
                _duplicates[duplicate].insert(innerDuplicate);
            }
        }
    }
}

/*!
 */
void DatabaseConnection::removeDuplicate(uint32_t userId)
{
    _duplicates[userId].clear();

    for (auto it = _duplicates.begin(); it != _duplicates.end(); ++it)
    {
        std::set<uint32_t> &set = it->second;
        set.erase(userId);
    }
}

/*!
 */
std::set<uint32_t> DatabaseConnection::getDuplicates(uint32_t userId)
{
    return _duplicates[userId];
}

/*!
 */
bool DatabaseConnection::knownUser(uint32_t userId) const
{
    return _players.contains(userId);
}

/*!
 */
const std::string& DatabaseConnection::ladder() const
{
    return _ladder;
}

/*!
 */
std::set<uint32_t> DatabaseConnection::getDeepDuplicateAccounts(uint32_t startUserId)
{
    std::set<uint32_t> visited;
    std::queue<uint32_t> queue;
    queue.push(startUserId);

    while (!queue.empty())
    {
        uint32_t currentId = queue.front();
        queue.pop();

        if (visited.contains(currentId))
        {
            continue;
        }

        visited.insert(currentId);

        std::unique_ptr<sql::PreparedStatement> stmt1(
            _connection->prepareStatement("SELECT ip_address_id FROM users WHERE id = ?")
            );

        stmt1->setUInt(1, currentId);
        std::unique_ptr<sql::ResultSet> res1(stmt1->executeQuery());

        int ipAddressId = -1;
        if (res1->next())
        {
            ipAddressId = res1->getInt("ip_address_id");
        }

        // Find other user_ids with same IP.
        std::set<uint32_t> ipRelated;
        if (ipAddressId >= 0)
        {
            std::unique_ptr<sql::PreparedStatement> stmt2(
                _connection->prepareStatement(
                    "SELECT user_id FROM ip_address_histories "
                    "WHERE ip_address_id = ? AND user_id != ?")
                );
            stmt2->setInt(1, ipAddressId);
            stmt2->setUInt(2, currentId);
            std::unique_ptr<sql::ResultSet> res2(stmt2->executeQuery());
            while (res2->next())
            {
                ipRelated.insert(res2->getUInt("user_id"));
            }
        }

        // Now find users with same qm_user_id.
        std::set<uint32_t> qmRelated;
        std::unique_ptr<sql::PreparedStatement> stmt3(
            _connection->prepareStatement("SELECT qm_user_id FROM qm_user_ids WHERE user_id = ?")
            );
        stmt3->setUInt(1, currentId);
        std::unique_ptr<sql::ResultSet> res3(stmt3->executeQuery());

        // Now look for qm client ids.
        std::vector<std::string> qmIds;
        while (res3->next())
        {
            qmIds.push_back(res3->getString("qm_user_id"));
        }

        if (!qmIds.empty())
        {
            std::string inClause(qmIds.size() * 2 - 1, '?');
            for (size_t i = 1; i < qmIds.size(); i++)
            {
                inClause[2 * i - 1] = ',';
            }

            std::string sql = "SELECT user_id FROM qm_user_ids WHERE qm_user_id IN (" + inClause + ") AND user_id != ?";
            std::unique_ptr<sql::PreparedStatement> statement4(_connection->prepareStatement(sql));

            int i = 1;

            for (const auto& id : qmIds)
            {
                statement4->setString(i++, id);
            }

            statement4->setUInt(i, currentId);

            std::unique_ptr<sql::ResultSet> result4(statement4->executeQuery());
            while (result4->next())
            {
                qmRelated.insert(result4->getUInt("user_id"));
            }
        }

        // Add all new ids to the queue, which are duplicated based on ip address.
        for (uint32_t uid : ipRelated)
        {
            if (!visited.contains(uid))
            {
                queue.push(uid);
            }
        }

        // Add all new ids to the queue, which are duplicated based on the qm client id.
        for (uint32_t uid : qmRelated)
        {
            if (!visited.contains(uid))
            {
                queue.push(uid);
            }
        }

    } // while (!queue.empty())

    return visited;
}

/*!
 */
std::set<uint32_t> DatabaseConnection::getWebLikeDuplicateAccounts(uint32_t userId)
{
    std::set<uint32_t> duplicates;

    int ipAddressId = -1;
    {
        std::unique_ptr<sql::PreparedStatement> statement(
            _connection->prepareStatement("SELECT ip_address_id FROM users WHERE id = ?")
            );
        statement->setUInt(1, userId);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
        if (result->next())
        {
            ipAddressId = result->getInt("ip_address_id");
        }
    }

    if (ipAddressId > 0)
    {
        std::unique_ptr<sql::PreparedStatement> statement(
            _connection->prepareStatement(
                "SELECT user_id FROM ip_address_histories "
                "WHERE ip_address_id = ? AND user_id != ?"
                )
            );
        statement->setInt(1, ipAddressId);
        statement->setUInt(2, userId);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());

        while (result->next())
        {
            duplicates.insert(result->getUInt("user_id"));
        }
    }

    return duplicates;
}

/*!
 */
uint32_t DatabaseConnection::loadPlayerFromAlias(const std::string &alias, Players &players)
{
    // Nicks for all ladders. Not used. Processing one ladder at once.
    std::unique_ptr<sql::PreparedStatement> statement(
        _connection->prepareStatement(
            "SELECT players.user_id, users.alias, players.username, users.name, ladders.abbreviation "
            "FROM players "
            "JOIN ladders ON players.ladder_id = ladders.id "
            "JOIN users ON users.id = players.user_id "
            "WHERE users.alias = ? "
            )
        );

    statement->setString(1, alias);
    std::unique_ptr<sql::ResultSet> result(statement->executeQuery());

    uint32_t userId = 0;
    std::optional<Player> player;

    Log::debug() << "Got " << (result->rowsCount() - 1) << " other player names for player " << alias << ".";

    while (result->next())
    {
        if (userId == 0)
        {
            userId = result->getInt("user_id");
            player = Player(userId, result->getString("name"), _gameMode);
            player->setAlias(alias);
        }

        // We are only interested in usernames from the current ladder.
        if (result->getString("abbreviation") == _ladder);
        {
            player->addName(result->getString("username"), result->getString("abbreviation"));
        }

        Log::info() << "User " << userId << " (" << result->getString("name") << ") has player name '" << result->getString("username") << "'.";
    }


    if (player)
    {
        players.add(*player, _ladder);
    }

    return userId;
}

/*!
 */
uint32_t DatabaseConnection::loadPlayer(
    const std::string &name,
    Players &players,
    const std::string &ladderAbbreviation)
{
    // Nicks for all ladders. Not used. Processing one ladder at once.
    /*
    std::unique_ptr<sql::PreparedStatement> statement(
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

    std::unique_ptr<sql::PreparedStatement> statement(
        _connection->prepareStatement(
            "SELECT players.user_id, players.username, ladders.abbreviation, users.alias, users.name "
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

    statement->setString(1, name);
    statement->setString(2, !ladderAbbreviation.empty() ? ladderAbbreviation : _ladder);
    statement->setString(3, !ladderAbbreviation.empty() ? ladderAbbreviation : _ladder);

    std::unique_ptr<sql::ResultSet> result(statement->executeQuery());

    uint32_t userId = 0;
    std::string alias;

    std::optional<Player> player;

    Log::debug() << "Got " << (result->rowsCount() - 1) << " other player names for player '" << name << "'";

    while (result->next())
    {
        if (userId == 0)
        {
            userId = result->getInt("user_id");
            player = Player(userId, result->getString("name"), _gameMode);
        }
        else if (userId != result->getInt("user_id") || player->account() != result->getString("name"))
        {
            Log::error() << "Found multiple users with qm name '" << name << '.';
            continue;
        }

        if (alias.empty() && !result->getString("alias").asStdString().empty())
        {
            alias = result->getString("alias");
            player->setAlias(alias);
        }

        // Do not give a warning if player name already existed if tournamend games were
        // added and the player has an aliad
        player->addName(result->getString("username"), result->getString("abbreviation"));

        Log::info() << "User " << userId << " (" << result->getString("name") << ") has player name '" << result->getString("username") << "'.";
    }

    if (player)
    {
        players.add(*player, !ladderAbbreviation.empty() ? ladderAbbreviation : _ladder);
    }

    return userId;
}

/*!
 */
bool DatabaseConnection::loadPlayerWithNoUser(
    uint32_t userId,
    Players &players,
    const std::string &ladderAbbreviation)
{
    std::unique_ptr<sql::PreparedStatement> stmt(
        _connection->prepareStatement(
            "SELECT players.user_id, players.username, ladders.abbreviation "
            "FROM players "
            "JOIN ladders ON players.ladder_id = ladders.id "
            "WHERE players.user_id = ? "
            // "AND ladders.abbreviation = ? " <= User might not have played the current ladder.
            "ORDER BY players.username;"
            )
        );

    stmt->setUInt(1, userId);
    // stmt->setString(2, _ladder);

    std::unique_ptr<sql::ResultSet> result(stmt->executeQuery());

    std::optional<Player> player;

    while (result->next())
    {
        if (!player.has_value())
        {
            player = Player(userId, "", _gameMode);
        }

        player->addName(result->getString("username"), result->getString("abbreviation"));
    }

    if (player)
    {
        players.add(*player, !ladderAbbreviation.empty() ? ladderAbbreviation : _ladder);
        return true;
    }

    return false;
}
/*!
 */
bool DatabaseConnection::loadPlayer(
    uint32_t userId,
    Players &players,
    const std::string &ladderAbbreviation)
{
    std::unique_ptr<sql::PreparedStatement> stmt(
        _connection->prepareStatement(
            "SELECT players.user_id, players.username, ladders.abbreviation, users.alias, users.name "
            "FROM players "
            "JOIN ladders ON players.ladder_id = ladders.id "
            "JOIN users ON players.user_id = users.id "
            "WHERE players.user_id = ? "
            "AND ladders.abbreviation = ? "
            "ORDER BY players.username;"
            )
        );

    stmt->setUInt(1, userId);
    stmt->setString(2, !ladderAbbreviation.empty() ? ladderAbbreviation : _ladder);

    std::unique_ptr<sql::ResultSet> result(stmt->executeQuery());

    std::optional<Player> player;

    while (result->next())
    {
        if (!player.has_value())
        {
            std::string accountName = result->getString("name");
            player = Player(userId, accountName, _gameMode);

            std::string alias = result->getString("alias");
            if (!alias.empty())
            {
                player->setAlias(alias);
            }
        }

        player->addName(result->getString("username"), result->getString("abbreviation"));
    }

    if (player)
    {
        players.add(*player, !ladderAbbreviation.empty() ? ladderAbbreviation : _ladder);
        Log::debug() << "Loaded player " << userId << ".";
        return true;
    }

    Log::debug() << "Unable to load player " << userId << " from table 'users'.";
    return false;
}

/*!
 */
std::map<uint32_t, Game> DatabaseConnection::fetchGames()
{
    std::map<uint32_t, Game> games;

    std::string ladderGames = R"sql(
        SELECT
            games.id AS gameId,
            players.username AS playerUsername,
            ladders.abbreviation AS ladderAbbreviation,
            player_game_reports.won AS playerWon,
            player_game_reports.points,
            sides.name AS playerCountry,
            COALESCE(maps.name, games.scen) AS map,
            game_reports.duration,
            game_reports.fps,
            UNIX_TIMESTAMP(games.created_at) AS timestamp,
            games.created_at AS played
        FROM games
        JOIN ladder_history ON games.ladder_history_id = ladder_history.id
        JOIN ladders ON ladder_history.ladder_id = ladders.id
        JOIN game_reports ON game_reports.id = games.game_report_id
        JOIN player_game_reports ON player_game_reports.game_report_id = games.game_report_id
        JOIN players ON players.id = player_game_reports.player_id
        JOIN stats2 ON stats2.id = player_game_reports.stats_id
        LEFT JOIN sides ON sides.local_id = stats2.cty AND sides.ladder_id = ladders.id
        LEFT JOIN qm_matches qmm ON qmm.id = games.qm_match_id
        LEFT JOIN qm_maps qmap ON qmm.qm_map_id = qmap.id
        LEFT JOIN maps maps ON maps.id = qmap.map_id
        WHERE ladders.abbreviation = ? AND games.created_at >= '2022-01-01'
        ORDER BY games.updated_at ASC;
    )sql";

    // sides.local_id = stats2.cty do not match for RA
    std::string ladderGamesRA = R"sql(
        SELECT
            games.id AS gameId,
            players.username AS playerUsername,
            ladders.abbreviation AS ladderAbbreviation,
            player_game_reports.won AS playerWon,
            player_game_reports.points,
            COALESCE(sides.name, 'Unbekannt') AS playerCountry,
            COALESCE(maps.name, games.scen) AS map,
            game_reports.duration,
            game_reports.fps,
            UNIX_TIMESTAMP(games.created_at) AS timestamp,
            games.created_at AS played
        FROM games
        JOIN ladder_history ON games.ladder_history_id = ladder_history.id
        JOIN ladders ON ladder_history.ladder_id = ladders.id
        JOIN game_reports ON game_reports.id = games.game_report_id
        JOIN player_game_reports ON player_game_reports.game_report_id = games.game_report_id
        JOIN players ON players.id = player_game_reports.player_id
        JOIN stats2 ON stats2.id = player_game_reports.stats_id
        LEFT JOIN sides ON sides.ladder_id = ladders.id AND sides.local_id = stats2.sid
        LEFT JOIN qm_matches qmm ON qmm.id = games.qm_match_id
        LEFT JOIN qm_maps qmap ON qmm.qm_map_id = qmap.id
        LEFT JOIN maps maps ON maps.id = qmap.map_id
        WHERE ladders.abbreviation = 'ra' AND games.created_at >= '2022-01-01'
        ORDER BY games.updated_at ASC
    )sql";

    // Games for ladder ra2 include ra2-new-maps and games playes between 2022-01-01 and 2022-05-01,
    // which have ladder abbreviation YR AND a map name in double brackets. At this time, cncnet
    // had an alternating ladder between ra2/yr, but games where tracked as yr

    std::string ladderGamesRA2(
            "SELECT "
            "  games.id AS gameId, "
            "  players.username AS playerUsername, "
            "  ladders.abbreviation AS ladderAbbreviation, "
            "  player_game_reports.won AS playerWon, "
            "  player_game_reports.points, "
            "  sides.name AS playerCountry, "
            "  COALESCE(maps.name, games.scen) AS map, "
            "  game_reports.duration, "
            "  game_reports.fps, "
            "  UNIX_TIMESTAMP(games.created_at) AS timestamp, "
            "  games.created_at AS played "
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
            "WHERE "
            "( "
            "  ladders.abbreviation IN ('ra2', 'ra2-new-maps') "
            "  OR "
            "  ( "
            "     ladders.abbreviation = 'yr' "
            "     AND games.created_at >= '2022-01-02' "
            "     AND games.created_at < '2022-05-01' "
            "     AND COALESCE(maps.name, games.scen) LIKE '\"%' "
            "     AND COALESCE(maps.name, games.scen) LIKE '%\"' "
            "   ) "
            ") "
            "ORDER BY games.updated_at ASC "
    );

    std::string ladderGamesYR(
        "SELECT "
        "  games.id AS gameId, "
        "  players.username AS playerUsername, "
        "  ladders.abbreviation AS ladderAbbreviation, "
        "  player_game_reports.won AS playerWon, "
        "  player_game_reports.points, "
        "  sides.name AS playerCountry, "
        "  COALESCE(maps.name, games.scen) AS map, "
        "  game_reports.duration, "
        "  game_reports.fps, "
        "  UNIX_TIMESTAMP(games.created_at) AS timestamp, "
        "  games.created_at AS played "
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
        "WHERE "
        "  ladders.abbreviation = 'yr' "
        "  AND games.created_at >= '2022-01-01' "
        "  AND NOT ( "
        "    games.created_at >= '2022-01-01' "
        "    AND games.created_at < '2022-05-01' "
        "    AND COALESCE(maps.name, games.scen) LIKE '\"%' "
        "    AND COALESCE(maps.name, games.scen) LIKE '%\"' "
        "  ) "
        "ORDER BY games.updated_at ASC "
        );

    std::string sqlStatement;

    if (_gameMode == gamemodes::RedAlert2)
    {
        sqlStatement = ladderGamesRA2;
    }
    else if (_gameMode == gamemodes::RedAlert)
    {
        sqlStatement = ladderGamesRA;
    }
    else if (_gameMode == gamemodes::YurisRevenge)
    {
        sqlStatement = ladderGamesYR;
    }
    else
    {
        sqlStatement = ladderGames;
    }

    std::unique_ptr<sql::PreparedStatement> statement(
        _connection->prepareStatement(sqlStatement));

    if (sqlStatement == ladderGames)
    {
        statement->setString(1, _ladder);
    }

    std::unique_ptr<sql::ResultSet> result(statement->executeQuery());

    while (result->next())
    {
        uint32_t gameId = static_cast<uint32_t>(result->getInt("gameId"));

        if (!games.contains(gameId))
        {
            uint32_t fps = static_cast<uint32_t>(result->getInt("fps"));
            uint32_t duration = static_cast<uint32_t>(result->getInt("duration"));
            uint32_t timestamp = static_cast<uint32_t>(result->getInt64("timestamp"));
            std::string map = result->getString("map");
            std::pair<std::map<uint32_t, Game>::iterator, bool> game = games.emplace(gameId, Game(gameId, map, timestamp, fps, duration));
            game.first->second.setGameType(gametypes::Quickmatch);
            game.first->second.setLadderAbbreviation(result->getString("ladderAbbreviation"));
        }

        auto it = games.find(gameId);
        Game *game = (it != games.end()) ? &(it->second) : nullptr;
        if (game == nullptr)
            continue;

        std::string playerName = result->getString("playerUsername");

        int32_t points = result->getInt("points");
        bool won = result->getBoolean("playerWon");
        std::string playerCountry = result->getString("playerCountry");

        factions::Faction faction = factions::fromName(playerCountry);
        if (faction == factions::UnknownFaction)
        {
            Log::warning() << "Cannot determine faction from '" << playerCountry << "'. Game " << gameId << " will probably be invalid.";
            continue;
        }

        game->addPlayer(0, playerName, faction, won, points, 0.0, 0.0);
    }

    return games;
}

/*!
 */
uint32_t DatabaseConnection::getBaseAccount(const std::set<uint32_t>& userIds, const std::set<uint32_t>& invalidUserIds)
{
    assert (!userIds.empty());
    uint32_t id = 0;
    std::string alias;

    std::string inClause(userIds.size() * 2 - 1, '?');
    for (size_t i = 1; i < userIds.size(); ++i)
    {
        inClause[2 * i - 1] = ',';
    }

    std::string sql = "SELECT id, alias FROM users WHERE id IN (" + inClause + ")";
    std::unique_ptr<sql::PreparedStatement> statement(_connection->prepareStatement(sql));

    int i = 1;
    for (uint32_t id : userIds)
    {
        statement->setUInt(i++, id);
    }

    std::unique_ptr<sql::ResultSet> result(statement->executeQuery());

    while (result->next())
    {
        std::string currentAlias = result->getString("alias");
        if (!currentAlias.empty())
        {
            if (!alias.empty())
            {
                Log::warning() << "User " << id << " with alias '" << alias << "' is a duplicate of user "
                             << result->getUInt("id") << " with alias '" << currentAlias << "'. Only one "
                             << "account is supposed to have an alias.";
            }
            else
            {
                id = result->getUInt("id");
                alias = currentAlias;
            }
        }
    }

    if (id != 0)
    {
        return id;
    }

    for (uint32_t userId : userIds)
    {
        if (!invalidUserIds.contains(userId))
        {
            return userId;
        }
    }

    Log::fatal() << "No base account found among user ids " << userIds << ".";
    throw std::runtime_error("No base account. Cannot continue;");
}
