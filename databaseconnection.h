 
#pragma once

#include <set>

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

    //! Init the duplicates.
    void initDuplicates();

    //! Get the ladder which is being worked on.
    const std::string& ladder() const;

    //! Add duplicates manually.
    void addDuplicates(uint32_t userId, std::set<uint32_t> duplicates);

    //! Remove a duplicate.
    void removeDuplicate(uint32_t userId);

    //! Get the duplicates for a user id. Duplicated must have been initialized prior.
    std::set<uint32_t> getDuplicates(uint32_t userId);

    //! Check if the given userId is known and has been checked for duplicates.
    bool knownUser(uint32_t userId) const;

    //! This returns the exact duplicates as shown on the website.
    std::set<uint32_t> getWebLikeDuplicateAccounts(uint32_t userId);

    //! Get all duplicated to the given user id. Will contain the given user id itself. Not used.
    std::set<uint32_t> getDeepDuplicateAccounts(uint32_t startUserId);

    //! Load a player by its alias. Only used to process tournament games with players, who never
    //! played quick match.
    uint32_t loadPlayerFromAlias(const std::string &alias, Players &players);

    //! Load a player from the database. Won't load duplicates.
    uint32_t loadPlayer(const std::string &name, Players &players, const std::string &ladderAbbreviation);

    //! Load a player from the database by its user_id.
    bool loadPlayer(uint32_t userId, Players &players, const std::string &ladderAbbreviation);

    //! Load a player from the database, who does not have an account anymore. This is probably
    //! a duplicate, that has been deleted. We still need all the player names assigned to the
    //! user id.
    bool loadPlayerWithNoUser(uint32_t userId, Players &players, const std::string &ladderAbbreviation);

    //! Add all recent games from the database. The player id won't be set for the games.
    std::map<uint32_t, Game> fetchGames();

    //! Get the base account to a list of user ids. This is the account with an alias
    //! or - if no alias exists - the account with the lowest number. Make sure not to use
    //! one of the invalid user ids.
    uint32_t getBaseAccount(const std::set<uint32_t>& userIds, const std::set<uint32_t> &invalidUserIds);

    //! Write data to table user_ratings;
    void writePlayerRatings(gamemodes::GameMode gameMode, const Players &players, std::map<uint32_t, uint32_t> activeRanks, std::map<uint32_t, uint32_t> allTimeRanks);

private:
    //! Is the connection ready?
    bool _ready;

    //! Ladder name (from table cncnet_api.ladders.abbreviation).
    std::string _ladder;

    //! Game mode (taken from ladder name).
    gamemodes::GameMode _gameMode;

    //! The sql connection.
    std::unique_ptr<sql::Connection> _connection;

    //! Map of duplicates.
    std::map<uint32_t, std::set<uint32_t>> _duplicates;

    //! List of players for this game mode.
    std::set<uint32_t> _players;

    //! Adding tournament games preloads players with alias. Used to not warn if player names already
    //! exist.
    bool _tournamtGamesAdded;
};
