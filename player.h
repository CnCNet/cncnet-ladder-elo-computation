 
#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <set>

#include "blitzmap.h"
#include "faction.h"
#include "game.h"
#include "gamemode.h"
#include "probabilities.h"
#include "rating.h"

struct PeakRating
{
    std::chrono::year_month_day date{std::chrono::year{0}, std::chrono::month{2}, std::chrono::day{31}};
    double adjustedElo = -1.0;
    double deviation;
    factions::Faction faction;
};

struct HighestRatedVictories
{
    uint32_t gameId;
    double ratingDifference;
    bool operator<(const HighestRatedVictories &hrd) const
    {
        // Want highest difference at the top.
        if (ratingDifference != hrd.ratingDifference)
        {
            return ratingDifference < hrd.ratingDifference;
        }
        else
        {
            return gameId < hrd.gameId;
        }
    }
};

struct LowestRatedDefeats
{
    uint32_t gameId;
    double ratingDifference;
    bool operator<(const LowestRatedDefeats &hrd) const
    {
        // Want highest difference at the top.
        if (ratingDifference != hrd.ratingDifference)
        {
            return ratingDifference < hrd.ratingDifference;
        }
        else
        {
            return gameId < hrd.gameId;
        }
    }
};

// Forward declarations:
class Players;

/*!
 * //Username// refers to the account name of the user.
 * //Alias// is the well-known name of this player. Once it is set, it cannot be changed anymore.
 * //Name// the name, which is used by the player in the monthly ladder. A player can basically have an
 *          unlimited number of names.
 */
class Player
{
public:
    //! Constructor.
    Player(uint32_t userId, const std::string &username, gamemodes::GameMode gameMode = gamemodes::Unknown);

    //! Constructor for illegal player.
    Player();

    //! Destructor.
    ~Player();

public:
    //! Get the user id.
    uint32_t userId() const;

    //! The the players account name.
    void setAccount(const std::string &account);

    //! The players account name.
    std::string account() const;

    //! Set the players alias (a.k.a tournament name).
    void setAlias(const std::string &alias);

    //! Check if the player has an alias.
    bool hasAlias() const;

    //! Get the players alias. Either return the raw alias (which might be empty)
    //! if player is not known to the community or return the most often used quick match
    //! name in case there is no alias.
    std::string alias() const;

    //! Number of wins.
    uint32_t wins() const;

    //! Number of losses.
    uint32_t losses() const;

    //! Number of draws.
    uint32_t draws() const;

    //! Get the rating for a specific faction.
    const Rating& rating(factions::Faction faction) const;

    //! Set the creation date of this player.
    void setCreationDate(const std::chrono::year_month_day &date);

    //! Get the date of creation of the player account.
    std::chrono::year_month_day creationDate() const;

    //! Add a player name used in ladder games.
    void addName(const std::string &name, const std::string &ladder);

    //! Get a list of all player names used in ladder games.
    const std::map<std::string, std::set<std::string>>& names() const;

    //! Increase the amount the given name has been used in the given game mode by 1.
    void increasePlayerNameUsage(const std::string &name);

    //! Get the rating of the player.
    double elo(factions::Faction faction) const;

    //! Get the rating deviation of the player.
    double deviation(factions::Faction faction) const;

    //! Get the glicko-2 volatility of the player.
    double volatility(factions::Faction faction) const;

    //! Process the given game.
    void processGame(const Game& game, int index, bool instantUpdate, const Players &players);

    //! Apply all pending games.
    void apply(std::chrono::year_month_day date, bool decay, gamemodes::GameMode gameMode);

    //! Update a players rating.
    Rating::CalculationType update();

    //! Check how many days can pass until this players becomes inactive.
    //! Returns 0 if the player is already inactive.
    int daysToInactivity(gamemodes::GameMode gameMode) const;

    //! Number of days since the last game was played.
    int daysFromLastGame() const;

    //! Number of days since the first game was plyed.
    int daysFromFirstGame() const;

    //! Number of days that player has been inactive.
    int daysInactive() const;

    //! Has this player ever been active?
    bool wasActive() const;

    //! Has this player ever been active for the given faction?
    bool wasActive(factions::Faction faction) const;

    //! Has this player been active for the given date?
    bool wasActiveBefore(const std::chrono::year_month_day &date, factions::Faction faction) const;

    //! The players best active faction.
    factions::Faction getBestActiveFaction() const;

    //! Get the maximum rating for this players. Return the rating of
    //! the highest active rating.
    double maxRating(bool includeInactive = false) const;

    //! Check if a specific faction is active.
    bool isActive(factions::Faction faction) const;

    //! Check if the player is active at all.
    bool isActive() const;

    //! Get the number of active days.
    int daysActive(bool sinceFirstActivation) const;

    //! Get the number of games played.
    uint32_t gameCount() const;

    //! Number of games pending.
    uint32_t pendingGameCount() const;

    //! Get the number of games played with the given faction.
    uint32_t gameCount(factions::Faction faction) const;

    //! Get the most often used player name in quick match.
    std::string mostOftenUsedPlayerName() const;

    //! Get the peak rating for a specific faction.
    PeakRating peakRating(factions::Faction faction) const;

    //! Get the overall peak rating.
    PeakRating peakRating() const;

    //! Date of the first game.
    std::chrono::year_month_day firstGame() const;

    //! Date of the last game.
    std::chrono::year_month_day lastGame() const;

    //! The number of overall days this player has been active.
    int daysActive() const;

    //! Finalize any kind of calculation.
    void finalize();

    //! Get the highest rated victories of this player.
    const std::set<HighestRatedVictories>& highestRatedVictories() const;

    //! Get the lowest rated defeats of this player.
    const std::set<LowestRatedDefeats>& lowestRatedDefeats() const;

    //! Get the stats against other players.
    const std::map<uint32_t, Probabilities>& vsOtherPlayers() const;

    //! Get the probabilities for a specific map.
    const Probabilities& mapStats(factions::Setup setup, int mapIndex) const;

    //! Get historical elo values for the given faction.
    std::map<uint32_t, std::pair<double, double> > historicalElo(factions::Faction faction) const;

    //! Check lexical order. Considers brackets.
    bool lowerLexicalOrder(const Player &other) const;

private:
    //! User id. 0 if invalid player.
    uint32_t _userId = 0;

    //! Number of wins.
    uint32_t _wins;

    //! Number of losses.
    uint32_t _losses;

    //! Number of draws.
    uint32_t _draws;

    //! Account name.
    std::string _account;

    //! The player's name used in tournament. Might be the most used quick match name, but
    //! can also a name, which was manually set.
    std::string _alias;

    //! Initial rating. Not present if player has never been active.
    std::optional<double> _initialRating = {};

    //! The number of game it took to become an active player for the first time.
    uint32_t _gamesToBecomeActive = 0;

    //! The date this account has been created.
    std::chrono::year_month_day _created;

    //! Quick match names, which are actually used along with the amount of times.
    std::map<std::string, uint32_t> _usedQmNames;

    //! The ratings for each faction.
    std::array<Rating, factions::count()> _ratings;

    //! A combined rating used for the deviation to track if
    //! a player is active or inactive.
    Rating _ratingCombined;

    //! The number of games played for each faction.
    std::array<uint32_t, factions::count()> _gameCount = { 0 };

    //! The peak ratings for each faction.
    std::array<PeakRating, factions::count()> _peakRatings;

    //! The date of the last game played.
    std::chrono::year_month_day _lastGame;

    //! The date of the first game played.
    std::chrono::year_month_day _firstGame;

    //! Pending games for each faction, including the combined rating.
    std::vector<std::array<double, 3>> _pendingGames[factions::count()];

    //! Pending results for each faction, including the combined results.
    std::vector<double> _pendingResults[factions::count()];

    //! Vector for each faction, which is set to true if at least one
    //! game was played in the current rating period. Used to add decay
    //! if no game was played.
    std::array<bool, factions::count()> _updated = { false };

    //! Status list for this player. Alternating dates where a player
    //! goes active (index 0, 2, ...) or inactive (index 1, 3, ...).
    std::vector<std::chrono::year_month_day> _statusList;

    //! Inactive/active dates for each faction. Way too complicated, but here's
    //! how it works. With the first date you go active, the second date is the
    //! day where you became inactive again. Odd number of values means you are
    //! active and an even number of values means you are inactive. Basically
    //! the same as above, but for each faction.
    std::array<std::vector<std::chrono::year_month_day>, factions::count()> _factionStatusList;

    //! ELO by date for each faction. First value of the pair is the elo, second
    //! the deviation. Key using std::chrono::year_month_day is not working properly.
    //! That's why its converted to uint32_t.
    std::map<uint32_t, std::array<std::pair<double, double>, factions::count()>> _eloByDate;

    //! List of highest rated victories. Stripped down to 20 elements max.
    std::set<HighestRatedVictories> _hightestRatedVictories;

    //! List of lowest rated victories. Stripped down to 20 elements max.
    std::set<LowestRatedDefeats> _lowestRatedDefeats;

    //! Statistics against each other player. Contains the expected and the actual result.
    std::map<uint32_t, Probabilities> _vsPlayer;

    //! Map statistics for each faction setup.
    std::array<std::array<Probabilities, blitzmap::count()>, factions::UnknownSetup> _mapStats;

    //! List of player names for each ladder.
    std::map<std::string, std::set<std::string>> _names;

}; // class Player

