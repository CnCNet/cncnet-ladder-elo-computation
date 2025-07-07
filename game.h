 
#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>

#include "faction.h"
#include "gametype.h"
#include "knownplayers.h"

/*!
 * Simple representation of a game. Does require an id, a map and a timestamp.
 * Frames per second (fps) and duration (in seconds) are optional.
 */
class Game
{
public:
    struct Participant
    {
        uint32_t userId;
        std::string playerName;
        factions::Faction faction;
        bool hasWon;
        int points;
        double elo;
        double deviation;
    };

    //! Constructor for a game.
    Game(uint32_t id, const std::string &map, uint32_t timestamp, uint32_t fps, uint32_t duration);

    //! Get the game id.
    uint32_t id() const;

    //! Get the number of players who participated in this game.
    uint32_t playerCount() const;

    //! Determine the winners based on points if no one of the participants has won.
    void determineWinner();

    //! Add a player to the game.
    void addPlayer(uint32_t index, const std::string &playerName, factions::Faction faction, bool hasWon, int points, double elo, double deviation);

    //! Result of a specific player. Index has to be 1 or 2.
    double result(uint32_t index) const;

    //! Set the player id of a specific player.
    void setPlayer(uint32_t index, uint32_t userId);

    //! Set the players. No sanity check is done. Player ids must be valid.
    void setPlayers(uint32_t player1, uint32_t player2);

    //! Get the game setup.
    factions::Setup setup() const;

    //! Do the given two players match in this game?
    bool isVs(uint32_t player1, uint32_t player2) const;

    //! Do the given two players match in this game?
    bool isVs(KnownPlayers player1, KnownPlayers player2) const;

    //! User id of the player with the given index.
    uint32_t userId(uint32_t index) const;

    //! Check if a player has won.
    bool hasWon(uint32_t index) const;

    //! Get name of player 1 or player 2.
    std::string playerName(uint32_t index) const;

    //! Set the timestamp (seconds sind 1/1/1970).
    void setTimestamp(uint32_t timestamp);

    //! Get the games' timestamp. Returns seconds since 1/1/1970.
    uint32_t timestamp() const;

    //! Set the average frames per second. Optional.
    void setFps(uint32_t fps);

    //! Get the average fps of the game. A value of 0 indicates that the fps is not known.
    uint32_t fps() const;

    //! Set the duration of the game in seconds.
    void setDuration(uint32_t seconds);

    //! Get the duration of the game in seconds. A value of 0 indicates, that
    //! no duration is available.
    uint32_t duration() const;

    //! Get a players faction. 0 is player 1, 1 is player 2.
    factions::Faction faction(uint32_t index) const;

    //! Set the ladder abbreviation for this game.
    void setLadderAbbreviation(const std::string &ladderAbbreviation);

    //! Get the ladder abbreviation for this game.
    std::string ladderAbbreviation() const;

    //! Set the map for this game.
    void setMap(uint32_t mapIndex);

    //! Get the map of this game.
    uint32_t map() const;

    //! Set the map name for this game. Optional information.
    void setMapName(const std::string &mapName);

    //! Get the map of this game.
    std::string mapName() const;

    //! Set the type of this game.
    void setGameType(gametypes::GameType gameType);

    //! Get the type of this game.
    gametypes::GameType gameType() const;

    //! Set disconnection state of the game.
    void setWasDisconnected(bool disconnected);

    //! Check if the game ended in a disconnect.
    bool wasDisconnected() const;

    //! Is this a game against a bot?
    bool isBot() const;

    //! Get the minimal rating difference for the two players.
    double differenceForGreatestDefeat() const;

    //! Did the lower rated player win?
    bool isUnderdogWin() const;

    //! Set rating and deviation for a player.
    void setRatingAndDeviation(uint32_t playerIndex, double rating, double deviation);

    //! Get the rating of the given player.
    double rating(uint32_t index) const;

    //! Get the absolut rating difference.
    double ratingDifference() const;

    //! Get the deviation of the given player.
    double deviation(uint32_t index) const;

    //! Get the date from the game.
    std::chrono::year_month_day date() const;

    //! Get the system date from the game.
    std::chrono::sys_days sysDate() const;

    //! Get the exact timestamp of the game.
    //std::tuple<std::chrono::year_month_day, std::chrono::hh_mm_ss<std::chrono::seconds>> dateTime() const;

    //! Check if the game is valid.
    bool isValid() const;

    //! Return the winner of the game. -1 for a draw, 0 for player 1, 1 for player 2.
    int winnerIndex() const;

    //! Get the index of the player with the given id. -1 if player has not been found.
    int playerIndex(uint32_t playerId) const;

    //! Check if the game is a draw.
    bool isDraw() const;

    //! Set if game was a draw.
    void setIsDraw(bool isDraw);

    //! Get index of team mate. Only works in 2v2 mode.
    uint32_t mateIndex(uint32_t index) const;

    //! Get index of both opponents in a 2v2 game.
    std::pair<uint32_t, uint32_t> opponentsIndices(uint32_t index) const;

    //! The faction result. Winning faction gets written in uppercase, so
    //! this function return something like 'avY' or 'Sva'.
    std::string factionResult(bool winnerFirst = false) const;

    //! Check if number of winning and losing players are equal.
    bool hasValidResult() const;

    //! Outputs the rating in Glicko-1 format.
    friend inline std::ostream& operator<<(std::ostream& os, const Game& game)
    {
        /*os << "["
           << game.id()
           << "] "
           << std::setfill('0')
           // TODO: << game.date() will work with GCC12
           << std::setw(4) << int(game.date().year()) << '-'
           << std::setw(2) << unsigned(game.date().month()) << '-'
           << std::setw(2) << unsigned(game.date().day())
           << " ";
        */
        std::chrono::sys_seconds tp{std::chrono::seconds{game._timestamp}};
        std::chrono::sys_days days = floor<std::chrono::days>(tp);
        std::chrono::year_month_day ymd{days};
        std::chrono::hh_mm_ss time{tp - days};

        os << "["
           << game.id()
           << "] "
           << std::setfill('0')
           << std::setw(4) << int(ymd.year()) << '-'
           << std::setw(2) << unsigned(ymd.month()) << '-'
           << std::setw(2) << unsigned(ymd.day()) << '@'
           << std::setw(2) << time.hours().count() << ':'
           << std::setw(2) << time.minutes().count() << '.'
           << std::setw(2) << time.seconds().count() << ' ';

        if (game._participants.size() == 2)
        {
            const Participant &p0 = game._participants[0];
            const Participant &p1 = game._participants[1];
            os << game.factionResult()
               << " "
               << p0.playerName << " [" << p0.userId<< "] (" << p0.elo << "/" << p0.deviation << ") vs "
               << p1.playerName << " [" << p1.userId << "] (" << p1.elo << "/" << p1.deviation << ") on "
               << game.mapName() << ": ";
            if (game.isDraw())
            {
                os << "0.5-0.5";
            }
            else
            {
                os << (game.hasWon(0) ? "1" : "0") << "-" << (game.hasWon(1) ? "1" : "0");
            }
        }
        else if (game._participants.size() == 4)
        {
            const Participant &p0 = game._participants[0];
            os << p0.playerName << " [" << p0.userId << "] (" << p0.elo << "/" << p0.deviation << ") + ";
            uint32_t mateIndex = game.mateIndex(0);
            // TODO: Check if valid.
            const Participant &p_mate = game._participants[mateIndex];
            os << p_mate.playerName << " [" << p_mate.userId << "] (" << p_mate.elo << "/" << p_mate.deviation << ") vs ";
            uint32_t firstOpponent;
            for (size_t i = 1; i < game._participants.size(); i++)
            {
                if (i != mateIndex)
                {
                    firstOpponent = i;
                    const Participant &o1 = game._participants[firstOpponent];
                    os << o1.playerName << " [" << o1.userId << "] (" << o1.elo << "/" << o1.deviation << ") + ";
                    break;
                }
            }
            for (size_t i = 1; i < game._participants.size(); i++)
            {
                if (i != mateIndex && i != firstOpponent)
                {
                    const Participant &o2 = game._participants[i];
                    os << o2.playerName << " [" << o2.userId << "] (" << o2.elo << "/" << o2.deviation << ") on ";
                    break;
                }
            }
            os << game.mapName() << ": ";
            os << (game.hasWon(0) ? "1" : "0") << "-" << (game.hasWon(0) ? "0" : "1");
        }
        return os;
    }

    //! Check if the given predicate matches all participants.
    template<typename Predicate>
    bool allParticipants(Predicate pred) const
    {
        return std::all_of(_participants.begin(), _participants.end(), pred);
    }

    //! Check if the given predicate matches at least one participant.
    template<typename Predicate>
    bool anyParticipant(Predicate pred) const
    {
        return std::any_of(_participants.begin(), _participants.end(), pred);
    }

    //! Collect data from participants.
    template<typename T, typename Func>
    std::vector<T> collectFromParticipants(Func func) const
    {
        std::vector<T> result;
        for (const auto& p : _participants)
        {
            auto [value, include] = func(p);
            if (include)
            {
                result.push_back(value);
            }
        }
        return result;
    }

private:
    //! Game type.
    gametypes::GameType _gameType = gametypes::Unknown;

    //! The game id. Uses the cncnet game id. Manually added tournament games start at 100.000.
    //! A game id of 0 indicates an invalid game.
    uint32_t _id = 0;

    //! The map this game was played on.
    uint32_t _map;

    //! Map name.
    std::string _mapName;

    //! The ladder abbreviation. This is usually the ladder we are processing, but it might
    //! differ. Some games on the yr-ladder are actual ra2 games. On top of that, ra2-new-maps
    //! are integrated into ra2 and we need to know which ladder to ask if the player name
    //! has to be resolved.
    std::string _ladderAbbreviation;

    //! Number of seconds since 1/1/1970 UTC. Might deviate by a couple of hours for
    //! tournament games, but won't affect rating, because games are submitted in
    //! daily batches. A timestamp must be provided in order for the algorithm to work
    //! properly
    uint32_t _timestamp = 0;

    //! Duration of the game in seconds. 0 if unknown.
    uint32_t _seconds = 0;

    //! The average fps count. 0 if unknown.
    uint32_t _fps = 0;

    //! Did this game end in a disconnect?
    bool _wasDisconnected = false;

    //! A game can also be drawn.
    bool _isDraw = false;

    //! List of participants in this game.
    std::vector<Participant> _participants;

}; // class game

