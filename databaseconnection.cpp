
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
void DatabaseConnection::removeDuplicate(std::map<uint32_t, std::set<uint32_t>> &duplicates, uint32_t userId)
{
    duplicates[userId].clear();

    Log::info() << "Removing duplicates for " << userId << ".";

    for (auto it = duplicates.begin(); it != duplicates.end(); ++it)
    {
        std::set<uint32_t> &set = it->second;
        if (set.contains(userId))
        {
            Log::info() << userId << " is not a duplicate of " << it->first << ".";
            set.erase(userId);
        }
    }
}

/*!
 */
const std::string& DatabaseConnection::ladder() const
{
    return _ladder;
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
            "SELECT players.user_id, users.primary_user_id, users.alias, players.username, users.name, ladders.abbreviation "
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
            uint32_t primaryUserId = result->getInt("primary_user_id");
            player = Player(userId, primaryUserId, result->getString("name"), _gameMode);
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
void DatabaseConnection::loadUsers(const std::set<uint32_t> &userIds, Players &players)
{
    if (userIds.empty())
    {
        Log::warning() << "No user ids provided.";
        return;
    }

    const size_t chunkSize = 500;
    auto it = userIds.begin();

    while (it != userIds.end())
    {
        std::vector<uint32_t> chunk;
        chunk.reserve(chunkSize);
        std::stringstream query;

        query << R"SQL(
            SELECT
                id,
                alias
            FROM users
                WHERE id IN (
        )SQL";

        for (size_t i = 0; i < chunkSize && it != userIds.end(); ++i, ++it)
        {
            query << ((i > 0) ? "," : "");
            query << "?";
            chunk.push_back(*it);
        }

        query << ");";

        std::unique_ptr<sql::PreparedStatement> stmt(_connection->prepareStatement(query.str()));
        for (size_t i = 0; i < chunk.size(); i++)
        {
            stmt->setUInt(i + 1, chunk[i]);
        }

        std::unique_ptr<sql::ResultSet> result(stmt->executeQuery());

        while (result->next())
        {
            uint32_t userId = result->getUInt("id");
            std::string alias = result->getString("alias");
            Player player(userId, userId, "", _gameMode);
            if (!alias.empty())
            {
                player.setAlias(alias);
            }
            players.add(player, _ladder);
        }
    }
}

/*!
 */
std::string DatabaseConnection::loadAlias(uint32_t userId)
{
    std::string sql = "SELECT id, alias FROM users WHERE id = ?;";
    std::unique_ptr<sql::PreparedStatement> statement(_connection->prepareStatement(sql));
    statement->setUInt(1, userId);
    std::unique_ptr<sql::ResultSet> result(statement->executeQuery());

    while (result->next())
    {
        std::string currentAlias = result->getString("alias");
        if (!currentAlias.empty())
        {
            return currentAlias;
        }
    }

    return std::string();
}

/*!
 */
std::unordered_map<uint32_t, uint32_t> DatabaseConnection::cncnetDuplicateMapping(const std::map<uint32_t, uint32_t> &userIds)
{
    std::unordered_map<uint32_t, uint32_t> result;
    std::map<uint32_t, std::set<uint32_t>> temporaryDuplicates;

    // First, use recent-IP algorithm to determine duplicates.
    for (const auto& [userId, gameCount] : userIds)
    {
        Log::verbose() << "User " << userId << " has played " << gameCount << " games.";
        temporaryDuplicates[userId] = std::set<uint32_t>();
        std::set<uint32_t> duplicates = getWebLikeDuplicateAccounts(userId);
        for (uint32_t duplicate : duplicates)
        {
            Log::verbose() << "User #" << duplicate << " is a duplicate of #" << userId << ".";
            temporaryDuplicates[userId].insert(duplicate);
            temporaryDuplicates[duplicate].insert(userId);

            for (uint32_t x : duplicates)
            {
                if (x != duplicate)
                {
                    temporaryDuplicates[duplicate].insert(x);
                }
            }
        }
    }

    // Next, add some well known duplicates, which are not detected by recent IP.
    temporaryDuplicates[  152].insert({ 37747, 79486 });
    temporaryDuplicates[  268].insert({    69 });
    temporaryDuplicates[ 3968].insert({ 18319, 66877 });
    temporaryDuplicates[17651].insert({ 40343, 43364, 44568 }); // This is actually wrong.
    temporaryDuplicates[19083].insert({ 10459 });
    temporaryDuplicates[33933].insert({ 300, 5878 });
    temporaryDuplicates[40500].insert({    24,  1029, 68169 });
    temporaryDuplicates[44616].insert({ 67416 });
    temporaryDuplicates[37077].insert({ 58873, 59236, 59916, 68898, 68942, 71304 });
    temporaryDuplicates[19548].insert({ 68698 });
    temporaryDuplicates[69904].insert({ 73057, 75285, 78280});
    temporaryDuplicates[47880].insert({ 71623 });
    temporaryDuplicates[53313].insert({ 59298, 76620 });
    temporaryDuplicates[54423].insert({ 20498 });
    temporaryDuplicates[55626].insert({ 73649 });
    temporaryDuplicates[58766].insert({ 58764, 66502 });
    temporaryDuplicates[59413].insert({   554, 61680 });
    temporaryDuplicates[60300].insert({ 61757, 65104, 65875 });
    temporaryDuplicates[62077].insert({ 56736 });
    temporaryDuplicates[63398].insert({ 63331 });
    temporaryDuplicates[67132].insert({ 1179 });
    temporaryDuplicates[67596].insert({ 36814 });
    temporaryDuplicates[60828].insert({ 77657, 74819});
    temporaryDuplicates[65311].insert({ 81488 });
    removeDuplicate(temporaryDuplicates, 56589);
    removeDuplicate(temporaryDuplicates, 6026);
    removeDuplicate(temporaryDuplicates, 58860);

    crunchDuplicates(temporaryDuplicates);

    // Ouput all duplicates.
    for (auto& [primaryUserId, duplicates] : temporaryDuplicates)
    {
        std::stringstream ss;
        for (uint32_t id : duplicates)
        {
            ss << " " << id;
        }
        Log::verbose() << "Duplicates of #" << primaryUserId << ":" << ss.str();
    }

    // Next, find the best primary account for each player.
    for (auto& [currentPrimary, duplicates] : temporaryDuplicates)
    {
        uint32_t currentBestPrimary = 0;
        uint32_t mostGames = 0;
        duplicates.insert(currentPrimary);

        for (uint32_t duplicate : duplicates)
        {
            std::string alias = loadAlias(duplicate);
            if (!alias.empty())
            {
                currentBestPrimary = duplicate;
                break;
            }

            auto it = userIds.find(duplicate);
            uint32_t gamesPlayed = (it == userIds.end()) ? 0 : it->second;

            if (gamesPlayed > mostGames)
            {
                mostGames = it->second;
                currentBestPrimary = duplicate;
            }
        }

        if (currentBestPrimary == 0)
        {
            Log::critical() << "No best primary account found.";
        }

        for (uint32_t duplicate : duplicates)
        {
            result[duplicate] = currentBestPrimary;
        }
    }

    return result;
}

/*!
 */
std::unordered_map<uint32_t, uint32_t> DatabaseConnection::duplicateToPrimaryMapping(const std::map<uint32_t, uint32_t> userIds)
{
    std::unordered_map<uint32_t, uint32_t> result;

    if (userIds.empty())
    {
        Log::error() << "No user ids for duplicate mapping.";
        return result;
    }

    const size_t chunkSize = 500;
    auto it = userIds.begin();
    while (it != userIds.end())
    {
        std::ostringstream query;
        query << R"sql(
            SELECT
                id,
                primary_user_id
            FROM users
            WHERE id IN (
        )sql";

        for (size_t i = 0; i < chunkSize && it != userIds.end(); ++i, ++it)
        {
            if (i > 0)
            {
                query << ",";
            }
            query << it->first;
        }
        query << ");";

        std::unique_ptr<sql::PreparedStatement> stmt(_connection->prepareStatement(query.str()));
        stmt->execute();
        std::unique_ptr<sql::ResultSet> queryResult(stmt->executeQuery());

        while (queryResult->next())
        {
            uint32_t userId = queryResult->getUInt("id");
            uint32_t primaryId = queryResult->getUInt("primary_user_id");
            if (primaryId != 0 && primaryId != userId)
            {
                result[userId] = primaryId;
            }
            else
            {
                result[userId] = userId;
            }
        }
    }

    return result;
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
            players.user_id AS playerUserId,
            ladders.abbreviation AS ladderAbbreviation,
            player_game_reports.won AS playerWon,
            player_game_reports.points,
            sides.name AS playerCountry,
            COALESCE(maps.name, games.scen, games.hash) AS map,
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

    // In contrast to other ladder, RA can start at 2020-01.
    // sides.local_id = stats2.cty do not match for RA
    std::string ladderGamesRA = R"sql(
        SELECT
            games.id AS gameId,
            players.username AS playerUsername,
            players.user_id AS playerUserId,
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
        WHERE ladders.abbreviation = 'ra' AND games.created_at >= '2020-01-01'
        ORDER BY games.updated_at ASC
    )sql";

    // Games for ladder ra2 include ra2-new-maps and games playes between 2022-01-01 and 2022-05-01,
    // which have ladder abbreviation YR AND a map name in double brackets. At this time, cncnet
    // had an alternating ladder between ra2/yr, but games where tracked as yr

    std::string ladderGamesRA2(
            "SELECT "
            "  games.id AS gameId, "
            "  players.username AS playerUsername, "
            "  players.user_id AS playerUserId, "
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
        "  players.user_id AS playerUserId, "
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
        uint32_t userId = result->getInt("playerUserId");
        bool won = result->getBoolean("playerWon");
        std::string playerCountry = result->getString("playerCountry");

        factions::Faction faction = factions::fromName(playerCountry);
        if (faction == factions::UnknownFaction)
        {
            Log::warning() << "Cannot determine faction from '" << playerCountry << "'. Game " << gameId << " will probably be invalid.";
            continue;
        }

        game->addPlayer(userId, playerName, faction, won, points, 0.0, 0.0);
    }

    return games;
}

/*!
 */
void DatabaseConnection::writePlayerRatings(
    gamemodes::GameMode gameMode,
    const Players &players,
    std::map<uint32_t, uint32_t> activeRanks,
    std::map<uint32_t, uint32_t> allTimeRanks)
{
    try
    {
        // Check if expected columns exist.
        std::set<std::string> requiredColumns = {
            "user_id", "ladder_id", "rating", "deviation", "elo_rank",
            "alltime_rank", "rated_games", "active", "created_at", "updated_at"
        };

        std::unique_ptr<sql::PreparedStatement> checkStmt(
            _connection->prepareStatement(R"SQL(
                SELECT COLUMN_NAME
                FROM information_schema.columns
                WHERE table_schema = DATABASE() AND table_name = 'user_ratings'
            )SQL")
        );

        std::unique_ptr<sql::ResultSet> columnResult(checkStmt->executeQuery());
        std::set<std::string> actualColumns;

        while (columnResult->next())
        {
            actualColumns.insert(columnResult->getString("COLUMN_NAME"));
        }

        for (const std::string &col : requiredColumns)
        {
            if (!actualColumns.contains(col))
            {
                Log::warning() << "Unable to write player ratings due to missing column '" << col << "' in 'user_ratings'.";
                return;
            }
        }

        // Get ladder id.
        uint32_t ladderId = 0;
        std::unique_ptr<sql::PreparedStatement> ladderStmt(
            _connection->prepareStatement("SELECT id FROM ladders WHERE abbreviation = ? LIMIT 1")
        );
        ladderStmt->setString(1, _ladder);
        std::unique_ptr<sql::ResultSet> ladderResult(ladderStmt->executeQuery());

        if (ladderResult->next())
        {
             ladderId = ladderResult->getUInt("id");
        }
        else
        {
            Log::fatal() << "Ladder '" << _ladder << "' not found in table 'ladders'.";
            return;
        }

        _connection->setAutoCommit(false);

        try
        {
            // Remove old entries.
            std::unique_ptr<sql::PreparedStatement> removeStmt(
                _connection->prepareStatement("DELETE FROM user_ratings WHERE ladder_id = ?")
            );
            removeStmt->setUInt(1, ladderId);
            removeStmt->execute();
            Log::info() << "Removed old entries from 'user_ratings'.";

            // Prepare statement to save user data.
            std::unique_ptr<sql::PreparedStatement> insertStmt(
                _connection->prepareStatement(R"SQL(
                    INSERT INTO user_ratings
                    (user_id, ladder_id, rating, deviation, elo_rank, alltime_rank, rated_games, active, created_at, updated_at)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, NOW(), NOW())
                )SQL")
            );

            for (uint32_t userId : players.userIds())
            {
                const Player &player = players[userId];
                uint32_t gameCount = player.gameCount();
                factions::Faction faction = player.getBestFaction(false);

                if (!player.isActive(faction) || gameMode == gamemodes::Blitz2v2)
                {
                    // Try to get any rating.
                    faction = factions::Combined;
                }

                insertStmt->setUInt(1, userId);
                insertStmt->setUInt(2, ladderId);
                insertStmt->setInt(3, static_cast<int>(std::round(player.elo(faction))));
                insertStmt->setInt(4, static_cast<int>(std::round(player.deviation(faction))));
                insertStmt->setUInt(5, activeRanks.contains(userId) ? activeRanks[userId] : 0);
                insertStmt->setUInt(6, allTimeRanks.contains(userId) ? allTimeRanks[userId] : 0);
                insertStmt->setUInt(7, gameCount);
                insertStmt->setBoolean(8, player.isActive());

                insertStmt->execute();
            }

            _connection->commit();
        }
        catch (sql::SQLException &e)
        {
            _connection->rollback();
            Log::fatal() << "Error while writing user ratings: " << e.what();
        }

        _connection->setAutoCommit(true);

        Log::info() << "Player ratings written to 'user_ratings'.";
    }
    catch (sql::SQLException &e)
    {
        Log::fatal() << "Error while writing user ratings: " << e.what();
    }
}

/*!
 */
void DatabaseConnection::crunchDuplicates(std::map<uint32_t, std::set<uint32_t>> &duplicates)
{
    // Unbalanced graph.
    std::map<uint32_t, std::set<uint32_t>> adj;
    for (const auto &kv : duplicates)
    {
        uint32_t k = kv.first;
        adj[k];
        for (uint32_t v : kv.second)
        {
            adj[k].insert(v);
            adj[v].insert(k);
        }
    }

    std::set<uint32_t> visited;
    std::map<uint32_t, std::set<uint32_t>> result;

    for (const auto &node : adj)
    {
        uint32_t start = node.first;
        if (visited.count(start)) continue;

        std::vector<uint32_t> stack = { start };
        std::set<uint32_t> component;

        while (!stack.empty())
        {
            uint32_t u = stack.back();
            stack.pop_back();
            if (visited.insert(u).second)
            {
                component.insert(u);
                for (uint32_t w : adj[u])
                {
                    if (!visited.count(w)) stack.push_back(w);
                }
            }
        }

        if (!component.empty())
        {
            uint32_t rep = *component.begin();
            std::set<uint32_t> others = component;
            others.erase(rep);
            result[rep] = std::move(others);
        }
    }

    duplicates = result;
}
