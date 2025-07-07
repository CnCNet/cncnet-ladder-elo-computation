 
#include <cmath>
#include <set>

#include "blitzmap.h"
#include "cplusplus.h"
#include "game.h"
#include "logging.h"
#include "stringtools.h"

/*!
 */
Game::Game(uint32_t id, const std::string &map, uint32_t timestamp, uint32_t fps, uint32_t duration) :
    _id(id),
    _mapName(map),
    _timestamp(timestamp),
    _fps(fps),
    _seconds(duration)
{
    // Nothing to do.
}

/*!
 */
uint32_t Game::id() const
{
    return _id;
}

/*!
 */
uint32_t Game::playerCount() const
{
    return static_cast<uint32_t>(_participants.size());
}

/*!
 */
void Game::determineWinner()
{
    int result = 0;

    for (Participant &participant : _participants)
    {
        result += participant.hasWon ? 1 : -1;
    }

    if (result == 0)
    {
        // Number of winners matches number of losers.
        return;
    }

    result = 0;

    // Try again based on points.
    for (Participant &participant : _participants)
    {
        result += (participant.points > 0) ? 1 : -1;
        participant.hasWon = participant.points > 0;
        Log::info() << "  Trying to determine winner. Player '" << participant.playerName << "' got " << participant.points << " points.";
    }

    Log::warning(result != 0) << "Unable to determine winners in game " << _id << ". This game will probably be invalid.";
}

/*!
 */
bool Game::isBot() const
{
    uint32_t bot = dts::to_underlying(KnownPlayers::BlitzBot);
    return std::find_if(_participants.begin(), _participants.end(),
                        [bot](const Participant& p) { return p.userId == bot; }) != _participants.end();
}

/*!
 */
bool Game::isVs(uint32_t player1, uint32_t player2) const
{
    if (_participants.size() != 2)
    {
        Log::warning() << "Not a 1v1 game.";
        return false;
    }

    return (_participants[0].userId == player1 && _participants[1].userId == player2)
           || (_participants[0].userId == player2 && _participants[1].userId == player1);
}

/*!
 */
bool Game::isVs(KnownPlayers player1, KnownPlayers player2) const
{
    return isVs(dts::to_underlying(player1), dts::to_underlying(player2));
}

/*!
 */
void Game::addPlayer(
    uint32_t index,
    const std::string &playerName,
    factions::Faction faction,
    bool hasWon,
    int points,
    double elo,
    double deviation)
{
    _participants.emplace_back(Participant{index, playerName, faction, hasWon, points, elo, deviation});
}

/*!
 */
uint32_t Game::userId(uint32_t index) const
{
    if (index >= static_cast<uint32_t>(_participants.size()))
    {
        Log::error() << "Asking for player " << index << ", but maximum index is " << (_participants.size() - 1) << ".";
        return 0;
    }

    return _participants[index].userId;
}

/*!
 */
bool Game::hasWon(uint32_t index) const
{
    if (index > playerCount())
    {
        Log::warning() << "Player index " << index << " out of range in game " << _id << ".";
        return false;
    }

    return _participants[index].hasWon;
}

/*!
 */
std::string Game::playerName(uint32_t index) const
{
    if (index >= static_cast<uint32_t>(_participants.size()))
    {
        Log::error() << "Player index " << index << " out of range in game " << _id << " while asking for name.";
        return "";
    }

    return _participants[index].playerName;
}

/*!
 */
void Game::setTimestamp(uint32_t timestamp)
{
    _timestamp = timestamp;
}

/*!
 */
uint32_t Game::timestamp() const
{
    return _timestamp;
}

/*!
 */
void Game::setLadderAbbreviation(const std::string &ladderAbbreviation)
{
    _ladderAbbreviation = ladderAbbreviation;
}

/*!
 */
std::string Game::ladderAbbreviation() const
{
    return _ladderAbbreviation;
}

/*!
 */
void Game::setPlayer(uint32_t index, uint32_t userId)
{
    if (index >= _participants.size())
    {
        Log::error() << "Cannot set player with index " << index << " in game " << _id
                     << ", because the game has only " << (_participants.size() + 1) << " players.";
        return;
    }

    _participants[index].userId = userId;
}

/*!
 */
std::chrono::year_month_day Game::date() const
{
    std::chrono::sys_seconds tp_since_epoch{std::chrono::seconds{_timestamp}};
    std::chrono::sys_days days = std::chrono::floor<std::chrono::days>(tp_since_epoch);
    return std::chrono::year_month_day{days};
}

/*!
 */
std::chrono::sys_days Game::sysDate() const
{
    using namespace std::chrono;
    auto date = this->date();

    return sys_days{ year{ date.year() } / month{ date.month() } / day{ date.day() } };
}

/*!
 */
/*
std::tuple<std::chrono::year_month_day, std::chrono::hh_mm_ss<std::chrono::seconds>> Game::dateTime() const
{
    using namespace std::chrono;

    sys_seconds tp_since_epoch{seconds{_timestamp}};

    sys_days days = floor<days>(tp_since_epoch);
    year_month_day ymd{days};

    hh_mm_ss hms{tp_since_epoch - days};

    return {ymd, hms};
}*/

/*!
 */
void Game::setDuration(uint32_t seconds)
{
    _seconds = seconds;
}

/*!
 */
uint32_t Game::duration() const
{
    return _seconds;
}

/*!
 */
void Game::setFps(uint32_t fps)
{
    _fps = fps;
}

/*!
 */
uint32_t Game::fps() const
{
    return _fps;
}

/*!
 */
factions::Faction Game::faction(uint32_t index) const
{
    if (index >= static_cast<uint32_t>(_participants.size()))
    {
        Log::error() << "Faction index of " << index << " is out of game for game " << _id << ".";
        return factions::UnknownFaction;
    }

    return _participants[index].faction;
}

/*!
 */
void Game::setMap(uint32_t mapIndex)
{
    _map = mapIndex;
}

/*!
 */
uint32_t Game::map() const
{
    return _map;
}

/*!
 */
void Game::setGameType(gametypes::GameType gameType)
{
    _gameType = gameType;
}

/*!
 */
gametypes::GameType Game::gameType() const
{
    return _gameType;
}

void Game::setMapName(const std::string &mapName)
{
    _mapName = mapName;

    // Need to set the map index as well.
    int mapIndex = blitzmap::toIndex(mapName);

    if (mapIndex < 0)
    {
        // TODO:
        // std::stringstream ss;
        // ss << "Unknown map '" << mapName.toStdString() << "' passed to Game::setMapName.";
        // throw std::runtime_error(ss.str());
    }

    _map = static_cast<uint32_t>(mapIndex);
}

/*!
 */
std::string Game::mapName() const
{
    return _mapName;
}

/*!
 */
bool Game::isValid() const
{
    if (_participants.size() == 4)
    {
        std::set<uint32_t> userIds;

        int result = 0;
        for (size_t i = 0; i < _participants.size(); i++)
        {
            if (_participants[i].userId == 0)
            {
                Log::info() << "Unable to resolve all player of game " << _id << ". This game is invalid.";
                return false;
            }
            result += _participants[i].hasWon ? 1 : -1;
            userIds.insert(_participants[i].userId);
        }
        if (result != 0)
        {
            Log::warning() << "Winning and losing players of game " << _id << " do not line up.";
            return false;
        }
        if (isDraw())
        {
            Log::warning() << "There is no draw in 2v2 games. Game " << _id << " is invalid.";
            return false;
        }
        if (userIds.size() != 4)
        {
            Log::info() << "Participants of game " << _id << " are duplicates. This game is invalid.";
            return false;
        }
        return _id != 0 && _timestamp != 0;
    }
    else if (_participants.size() == 2)
    {
        if (_participants[0].userId == _participants[1].userId)
        {
            Log::info() << "Game " << _id << " is between duplicates. This game is invalid.";
            return false;
        }

        bool isValid = (
            _id != 0 &&
            _timestamp != 0 &&
            _participants[0].userId != 0 &&
            _participants[1].userId != 0 &&
            (_isDraw || ((_participants[0].hasWon && !_participants[1].hasWon) || (!_participants[0].hasWon && _participants[1].hasWon))));
        return isValid;
    }

    return false;
}

/*!
 */
void Game::setWasDisconnected(bool disconnected)
{
    _wasDisconnected = disconnected;
}

/*!
 */
bool Game::wasDisconnected() const
{
    return _wasDisconnected;
}

/*!
 */
double Game::differenceForGreatestDefeat() const
{
    if (_isDraw)
    {
        return 0.0;
    }

    double winnerElo = 0.0;
    double loserElo = 0.0;

    for (const Participant &participant : _participants)
    {
        if (participant.hasWon)
        {
            winnerElo += (participant.elo + participant.deviation);
        }
        else
        {
            loserElo += (participant.elo - participant.deviation);
        }
    }

    return loserElo - winnerElo;
}

/*!
 */
bool Game::isUnderdogWin() const
{
    if (_participants.size() != 2)
    {
        Log::error() << "Underdog win is only viable for a 1v1 game.";
        return 0.0;
    }

    if (_isDraw)
    {
        return false;
    }
    else
    {
        return (_participants[0].hasWon && _participants[0].elo < _participants[1].elo)
               || (_participants[1].hasWon && _participants[1].elo < _participants[0].elo);
    }
}

bool Game::hasValidResult() const
{
    if (_isDraw)
    {
        return true;
    }

    // Number of winning and losing players need to be equal in order
    // to have a valid game result.

    int result = 0;

    for (size_t i = 0; i < _participants.size(); i++)
    {
        result += _participants[i].hasWon ? 1 : -1;
    }

    return (result == 0);
}



/*!
 */
std::string Game::factionResult(bool winnerFirst) const
{
    if (_participants.size() != 2)
    {
        Log::error() << "Faction result is only viable for a 1v1 game.";
        return "";
    }

    std::string faction1 = factions::letter(_participants[0].faction);
    std::string faction2 = factions::letter(_participants[1].faction);

    if (_participants[0].hasWon)
    {
        faction1 = stringtools::toUpper(faction1);
    }
    else if (_participants[1].hasWon)
    {
        faction2 = stringtools::toUpper(faction2);
    }

    if (winnerFirst)
    {
        if (_participants[0].hasWon)
        {
            return faction1 + "v" + faction2;
        }
        else
        {
            return faction2 + "v" + faction1;
        }
    }

    return faction1 + "v" + faction2;
}

/*!
 */
void Game::setRatingAndDeviation(uint32_t playerIndex, double rating, double deviation)
{
    if (playerIndex >= playerCount())
    {
        Log::error() << "Player index " << playerIndex << " is out of bounds.";
    }
    else
    {
        _participants[playerIndex].elo = rating;
        _participants[playerIndex].deviation = deviation;
    }
}

/*!
 */
double Game::rating(uint32_t index) const
{
    if (index >= static_cast<uint32_t>(_participants.size()))
    {
        Log::error() << "Index " << index << " is not within [0, " << (_participants.size() - 1) << "] while getting the players rating of a game.";
    }

    return _participants[index].elo;
}

/*!
 */
double Game::ratingDifference() const
{
    if (_participants.size() != 2)
    {
        Log::error() << "Rating difference is only viable for a 1v1 game.";
        return 0.0;
    }
    return std::abs(_participants[0].elo - _participants[1].elo);
}

/*!
 */
double Game::deviation(uint32_t index) const
{
    if (index >= static_cast<uint32_t>(_participants.size()))
    {
        Log::error() << "Index " << index << " is not within [0, " << (_participants.size() - 1) << "] while getting the players deviation of a game.";
    }

    return _participants[index].deviation;
}

/*!
 */
int Game::winnerIndex() const
{
    if (_participants.size() != 2)
    {
        Log::error() << "Only 1v1 games have a winner index.";
        return -1;
    }
    else if (_isDraw)
    {
        Log::warning() << "Asking for winner index in a drawn game.";
        return -1;
    }

    if (_participants[0].hasWon)
    {
        return 0;
    }
    else if (_participants[1].hasWon)
    {
        return 1;
    }
    else
    {
        Log::warning() << "Unable to determine winner in game " << _id << ".";
        return -1;
    }
}

int Game::playerIndex(uint32_t playerId) const
{
    for (size_t i = 0; i < _participants.size(); i++)
    {
        if (_participants[i].userId == playerId)
        {
            return i;
        }
    }

    return -1;
}

/*!
 */
bool Game::isDraw() const
{
    return _isDraw;
}

/*!
 */
void Game::setIsDraw(bool isDraw)
{
    _isDraw = isDraw;
}

/*!
 */
factions::Setup Game::setup() const
{
    if (_participants.size() != 2)
    {
        Log::error() << "Factions setup is only viable for a 1v1 game.";
        return factions::Setup::UnknownSetup;
    }

    return factions::fromFactions(_participants[0].faction, _participants[1].faction);
}

/*!
 */
uint32_t Game::mateIndex(uint32_t index) const
{
    if (_participants.size() != 4)
    {
        Log::error() << "Only 2v2 game has a mate.";
        return index;
    }

    for (size_t i = 0; i < _participants.size(); i++)
    {
        if (i != index && _participants[i].hasWon == _participants[index].hasWon)
        {
            return i;
        }
    }

    Log::error() << "Mate not found in game " << _id << ".";
    return index;
}

std::pair<uint32_t, uint32_t> Game::opponentsIndices(uint32_t index) const
{
    if (playerCount() != 4)
    {
        Log::error() << "Opponents index only viable for a 2v2 game.";
        return {index, index};
    }

    bool isFirst = true;
    std::pair<uint32_t, uint32_t> result;

    for (size_t i = 0; i < _participants.size(); i++)
    {
        if (_participants[i].hasWon != _participants[index].hasWon)
        {
            if (isFirst)
            {
                result.first = i;
                isFirst = false;
            }
            else
            {
                result.second = i;
                return result;
            }
        }
    }

    Log::error() << "Opponents not found in game " << _id << ". This does not seems to be a valid 2v2 game.";
    return {index, index};
}
