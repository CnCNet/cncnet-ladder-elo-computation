 
#pragma once

#include <set>
#include <unordered_set>

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <nlohmann/json.hpp>

#include "logging.h"
#include "options.h"
#include "player.h"

class DatabaseConnection
{
public:
    //! Constructor. Establishes a connection. The ladder has to fixed and can't be changed later on.
    DatabaseConnection(const Options &options);

    //! Check if the connection is established.
    bool isEstablished() const;

    //! Check if the given ladder exists.
    bool ladderExists(const std::string &abbreviation) const;

    //! Get the ladder which is being worked on.
    const std::string& ladder() const;

    //! Remove a duplicate.
    void removeDuplicate(std::map<uint32_t, std::set<uint32_t> > &duplicates, uint32_t userId);

    //! This returns the exact duplicates as shown on the website.
    std::set<uint32_t> getWebLikeDuplicateAccounts(uint32_t userId);

    //! Load a player by its alias. Only used to process tournament games with players, who never
    //! played quick match.
    uint32_t loadPlayerFromAlias(const std::string &alias, Players &players);

    //! Load alias from database for the given user id.
    std::string loadAlias(uint32_t userId);

    //! Load all the users with the given user ids and create players. Basically only load the
    //! alias for each player.
    void loadUsers(const std::set<uint32_t> &userIds, Players &players);

    //! Add all recent games from the database. The player id won't be set for the games.
    std::map<uint32_t, Game> fetchGames();

    //! Get a map to get the primary user id (value) for each user (key):
    std::unordered_map<uint32_t, uint32_t> duplicateToPrimaryMapping(const std::map<uint32_t, uint32_t> userIds);

    //! Return a map with the key being the duplicate account and the value being the primary account.
    std::unordered_map<uint32_t, uint32_t> cncnetDuplicateMapping(const std::map<uint32_t, uint32_t> &userIds);

    //! Write data to table user_ratings;
    void writePlayerRatings(gamemodes::GameMode gameMode, const Players &players, std::map<uint32_t, uint32_t> activeRanks, std::map<uint32_t, uint32_t> allTimeRanks);

    //! Create the transitive closure for all duplicates.
    void crunchDuplicates(std::map<uint32_t, std::set<uint32_t>> &duplicates);

private:
    //! Is the connection ready?
    bool _ready;

    //! Ladder name (from table cncnet_api.ladders.abbreviation).
    std::string _ladder;

    //! Game mode (taken from ladder name).
    gamemodes::GameMode _gameMode;

    //! The sql connection.
    std::unique_ptr<sql::Connection> _connection;

    //! Adding tournament games preloads players with alias. Used to not warn if player names already
    //! exist.
    bool _tournamtGamesAdded;
};
