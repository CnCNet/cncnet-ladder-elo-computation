
#pragma once

#include <array>

#include <nlohmann/json.hpp>

#include "faction.h"
#include "gamemode.h"
#include "player.h"

// Forward declarations:
class Games;

class Players
{
public:
    //! Constructor.
    Players();

    //! Get the number of players.
    size_t count() const;

    //! Check if a player exists.
    bool contains(uint32_t userId) const;

    //! Does this player name already exist?
    bool exists(const std::string &playerName);

    //! Get the index of a player based on the quick match name.
    //! Returns 0 if no player is found.
    uint32_t userId(const std::string &nick, const std::string &ladderName) const;

    //! Get the index of a player based on his alias.
    //! Returns 0 if no player is found.
    uint32_t userIdFromAlias(const std::string &alias) const;

    //! Add a players. Needs to have a valid user id.
    void add(const Player &player, const std::string &ladderAbbreviation);

    //! Array subscript operator. Returns player by user id.
    Player& operator[](uint32_t index);

    //! Const array subscript operator. Returns player by user id.
    Player operator[](uint32_t index) const;

    //! Get the number of currently active players.
    uint32_t activePlayerCount() const;

    //! Link the duplicates the given user id.
    void markDuplicates(uint32_t id, const std::set<uint32_t>& duplicates);

    //! Is this account a test account?
    bool isTestAccount(uint32_t userId) const;

    //! Update the ratings.
    void update();

    //! Finalize player calculations.
    void finalize();

    //! Apply pending games to all players.
    void apply(std::chrono::year_month_day date, bool decay, gamemodes::GameMode gameMode);

    // Exporting methods for actual rankings.
public:
    //! Export list of active players, sorted by rating. The is the actual ELO list.
    void exportActivePlayers(const std::filesystem::path &directory, gamemodes::GameMode gameMode) const;

    //! Make list of best players of all time.
    void exportBestOfAllTime(const std::filesystem::path &directory, gamemodes::GameMode gameMode) const;

    //! Most active players aka most loyal players.
    void exportMostDaysActive(const std::filesystem::path &directory, gamemodes::GameMode gameMode) const;

    //! Export all players in alphabetical order, who are or were active.
    void exportAlphabeticalOrder(const std::filesystem::path &directory, gamemodes::GameMode gameMode) const;

    //! Export new players, who have not been active so far.
    void exportNewPlayers(const std::filesystem::path &directory, gamemodes::GameMode gameMode) const;

    //! Export detailed information about each player.
    void exportPlayerDetails(const std::filesystem::path &directory, std::vector<uint32_t> userIds, const std::map<uint32_t, Game> &games, const std::string &ladderAbbreviation) const;

private:
    //! Players ordered by their user id.
    std::map<uint32_t, Player> _players;

private:
    //! Get the user id from a nick. First key is the ladder.
    std::map<std::string, std::map<std::string, uint32_t>> _nickToUserId;

    //! List of duplicates to each user id.
    std::map<uint32_t, uint32_t> _duplicates;

    //! List of user ids, which are just test accounts.
    std::set<uint32_t> _testAccounts;

}; // class Players

