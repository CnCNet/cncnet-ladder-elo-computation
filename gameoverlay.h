
#pragma once

#include <filesystem>

#include "databaseconnection.h"
#include "players.h"

/*!
 * The GameOverlay class fixed the result of some games and can add
 * additional games to the pool.
 */
class GameOverlay
{

public:
    //! Load additional tournament games.
    void loadTournamentGames(DatabaseConnection &connection, const std::filesystem::path &file, Players &players, gamemodes::GameMode gameMode, const std::string &ladderAbbreviation, std::map<uint32_t, Game> &games);


}; // class GameOverlay
